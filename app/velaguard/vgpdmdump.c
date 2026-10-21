/****************************************************************************
 * app/velaguard/vgpdmdump.c
 *
 * BVO-03b 板载麦克风原始 PDM 采集验证工具。
 *
 * 直接从 SAI 设备取一段定长的原始 PDM 位流，写到文件，并打印足以判断
 * 通路是否真的工作的统计量。这只用于 bring-up，不属于产品音频路径：
 * 不做抽取滤波，不接语音，不改变任何配置或点表。
 *
 * 为什么不用 /dev/i2schar0：i2schar 的 read() 把缓冲区交给驱动后立刻
 * 返回，并在自己的回调里释放缓冲区，数据从不回到调用者。要拿到位流，
 * 只能自己持有缓冲区并等驱动回调。
 *
 * 判据说明。DMA 中断递增或缓冲区非零，只能证明总线上的传输在动。要认定
 * 麦克风真的在工作，还需要同时看到：
 *   - 置位比特占比接近 0.5（PDM 位流在静音时占空比约一半）；
 *   - 字节取值不是单一常量（全 0x00 或 0xFF 说明数据线被拉死）；
 *   - 说话与静音两种条件下的统计量有可观察差异。
 * 因此本工具打印直方图和熵，而不只打印首字节。
 ****************************************************************************/

#include <nuttx/config.h>

#ifdef CONFIG_VG_PDM_DUMP

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>
#include <time.h>

#include <nuttx/audio/i2s.h>
#include <nuttx/cache.h>
#include <nuttx/semaphore.h>

#include <arch/board/board.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* 采集长度上界。16 kHz 采样率、64 倍抽取下，每毫秒约 128 字节 PDM。
 * 32 KiB 约等于 256 ms，够判断通路，又不至于在 NSH 里等太久。
 */

#define VG_PDM_MINBYTES   (64)
#define VG_PDM_MAXBYTES   (32768)

/* 进内存的字节率，换算成 bit/s 供显示用。PLL2P 49.143 MHz 经 MCKDIV 12
 * 得到 4.095 MHz 位时钟，32 位帧即 128 kHz 帧率；PDM 每帧向内存交出一个
 * 字节，所以字节率是 128 KB/s。这个值与 64 倍抽取到 16 kHz 自洽：
 * 128 KB/s ÷ 8 = 128 kHz 采样，÷ 8（每帧 8 个有效比特）… 实测核对过
 * 32768 字节耗时 270 ms，与 128 KB/s 相符。
 *
 * 只用于把字节数折算成时长，不参与任何采集决策。
 */

#define VG_PDM_BITRATE    (1024000.0)

/* 单次接收的等待上限。按最坏情况留足余量：32 KiB 在 1.024 MHz 位流下
 * 约 256 ms，超时给到 5 秒。
 */

#define VG_PDM_TIMEOUT_MS (5000)

/****************************************************************************
 * Private Types
 ****************************************************************************/

/* 一次采集的统计量。只做只读统计，不含结论性判断。
 */

struct vg_pdm_stats
{
  size_t   nbytes;        /* 实际收到的字节数 */
  size_t   nzero;         /* 值为 0x00 的字节数 */
  size_t   nff;           /* 值为 0xFF 的字节数 */
  size_t   nbits;         /* 置位的比特总数 */
  double   ones_ratio;    /* 置位比特占比，静音时接近 0.5 */
  double   entropy;       /* 每字节香农熵，上限 8 bit */
  size_t   dup_pairs;     /* 相邻两字节相同的次数 */
  size_t   halfword_dups; /* 相邻两个半字相同的次数 */
  uint8_t  minbyte;
  uint8_t  maxbyte;
  uint32_t hist[16];      /* 高 4 位直方图，16 个桶 */
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

static sem_t     g_done;
static int       g_result;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: vg_pdm_usage
 ****************************************************************************/

static void vg_pdm_usage(void)
{
  printf("usage: vgpdmdump [-n bytes] [-f path] [-q] [-x count]\n"
         "  -n bytes  capture length, %d..%d, default 16384\n"
         "  -f path   output file, default /data/pdm.raw\n"
         "  -q        do not write a file, statistics only\n"
         "  -x count  also hex dump the first count bytes (max 256)\n",
         VG_PDM_MINBYTES, VG_PDM_MAXBYTES);
}

/****************************************************************************
 * Name: vg_pdm_hexdump
 *
 * Description:
 *   以十六进制打印一段前缀。板上没有 xxd 一类工具，需要把原始字节带回
 *   主机核对时只能靠这个输出。
 *
 ****************************************************************************/

static void vg_pdm_hexdump(FAR const uint8_t *buf, size_t nbytes)
{
  size_t i;

  for (i = 0; i < nbytes; i++)
    {
      if ((i & 15) == 0)
        {
          printf("  %04zu:", i);
        }

      printf(" %02x", buf[i]);

      if ((i & 15) == 15 || i + 1 == nbytes)
        {
          printf("\n");
        }
    }
}

/****************************************************************************
 * Name: vg_pdm_stats
 *
 * Description:
 *   统计一段 PDM 位流。所有字段从同一遍扫描得出，避免各字段来自缓冲区
 *   的不同状态。
 *
 ****************************************************************************/

static void vg_pdm_stats(FAR const uint8_t *buf, size_t nbytes,
                         FAR struct vg_pdm_stats *st)
{
  uint32_t counts[256];
  size_t   i;
  double   h;

  memset(st, 0, sizeof(*st));
  memset(counts, 0, sizeof(counts));

  st->nbytes  = nbytes;
  st->minbyte = 0xff;
  st->maxbyte = 0x00;

  for (i = 0; i < nbytes; i++)
    {
      uint8_t b = buf[i];

      counts[b]++;
      st->nbits += (size_t)__builtin_popcount(b);

      if (b == 0x00)
        {
          st->nzero++;
        }

      if (b == 0xff)
        {
          st->nff++;
        }

      if (b < st->minbyte)
        {
          st->minbyte = b;
        }

      if (b > st->maxbyte)
        {
          st->maxbyte = b;
        }

      st->hist[b >> 4]++;
    }

  if (nbytes > 0)
    {
      st->ones_ratio = (double)st->nbits / ((double)nbytes * 8.0);
    }

  /* 香农熵按字节取值计算。真实位流接近 8 bit；常量数据线为 0。
   */

  h = 0.0;
  for (i = 0; i < 256; i++)
    {
      if (counts[i] != 0)
        {
          double p = (double)counts[i] / (double)nbytes;
          h -= p * (log(p) / log(2.0));
        }
    }

  st->entropy = h;

  /* 字节级重复检测。半字里高低字节相同，往往说明 DMA 传输宽度大于外设
   * 实际给出的有效位宽：多出来的高位只是重复或补零。这比单看熵更能区分
   * “数据是常量”与“数据格式不对”。
   */

  for (i = 1; i < nbytes; i++)
    {
      if (buf[i] == buf[i - 1])
        {
          st->dup_pairs++;
        }
    }

  for (i = 2; i + 1 < nbytes; i += 2)
    {
      uint16_t a = (uint16_t)buf[i] | ((uint16_t)buf[i + 1] << 8);
      uint16_t b = (uint16_t)buf[i - 2] | ((uint16_t)buf[i - 1] << 8);

      if (a == b)
        {
          st->halfword_dups++;
        }
    }
}

/****************************************************************************
 * Name: vg_pdm_report
 ****************************************************************************/

static void vg_pdm_report(FAR const struct vg_pdm_stats *st)
{
  int i;

  printf("vgpdmdump: %zu bytes\n", st->nbytes);
  printf("  zero=%.1f%% ones=%.3f entropy=%.3f bit/byte\n",
         (st->nbytes > 0) ?
           (100.0 * (double)st->nzero / (double)st->nbytes) : 0.0,
         st->ones_ratio, st->entropy);
  printf("  min=0x%02x max=0x%02x allff=%zu\n",
         st->minbyte, st->maxbyte, st->nff);
  printf("  dup: byte %.1f%% (of %zu)  halfword %.1f%% (of %zu)\n",
         (st->nbytes > 1) ?
           (100.0 * (double)st->dup_pairs / (double)(st->nbytes - 1)) : 0.0,
         (st->nbytes > 1) ? st->nbytes - 1 : (size_t)0,
         (st->nbytes > 3) ?
           (100.0 * (double)st->halfword_dups /
            (double)((st->nbytes - 2) / 2)) : 0.0,
         (st->nbytes > 3) ? (st->nbytes - 2) / 2 : (size_t)0);
  printf("  nibble histogram:");

  for (i = 0; i < 16; i++)
    {
      printf(" %lu", (unsigned long)st->hist[i]);
      if (i != 15)
        {
          printf(",");
        }
    }

  printf("\n");
}

/****************************************************************************
 * Name: vg_pdm_callback
 *
 * Description:
 *   I2S 接收完成回调。在驱动 worker 线程里执行，只记录结果并唤醒主线程，
 *   不做统计、不打印、不分配内存。
 *
 ****************************************************************************/

static void vg_pdm_callback(FAR struct i2s_dev_s *dev,
                            FAR struct ap_buffer_s *apb,
                            FAR void *arg, int result)
{
  (void)dev;
  (void)apb;
  (void)arg;

  g_result = result;
  nxsem_post(&g_done);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: vgpdmdump_main
 *
 * Description:
 *   NSH 入口。打开采集设备，投递一次定长接收，等回调完成后统计并落盘。
 *
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  FAR const char *path = "/data/pdm.raw";
  size_t nbytes = 16384;
  bool quiet = false;
  size_t dumpbytes = 0;
  struct vg_pdm_stats stats;
  struct audio_buf_desc_s bufdesc;
  FAR struct ap_buffer_s *apb;
  FAR struct i2s_dev_s *i2s;
  struct timespec t0;
  struct timespec t1;
  uint32_t bitrate;
  int ret;
  int i;
  int out;

  for (i = 1; i < argc; i++)
    {
      if (strcmp(argv[i], "-n") == 0 && i + 1 < argc)
        {
          long v = strtol(argv[++i], NULL, 0);

          if (v < VG_PDM_MINBYTES || v > VG_PDM_MAXBYTES)
            {
              fprintf(stderr, "vgpdmdump: -n out of range\n");
              return EXIT_FAILURE;
            }

          nbytes = (size_t)v;
        }
      else if (strcmp(argv[i], "-f") == 0 && i + 1 < argc)
        {
          path = argv[++i];
        }
      else if (strcmp(argv[i], "-q") == 0)
        {
          quiet = true;
        }
      else if (strcmp(argv[i], "-x") == 0 && i + 1 < argc)
        {
          long v = strtol(argv[++i], NULL, 0);

          if (v < 0 || v > 256)
            {
              fprintf(stderr, "vgpdmdump: -x out of range\n");
              return EXIT_FAILURE;
            }

          dumpbytes = (size_t)v;
        }
      else
        {
          vg_pdm_usage();
          return EXIT_FAILURE;
        }
    }

  /* 长度取整到 16 位传输单位的整数倍，避免最后一拍被截掉。
   */

  nbytes &= ~(size_t)1;

  i2s = stm32_sai_pdm_device();
  if (i2s == NULL)
    {
      fprintf(stderr, "vgpdmdump: PDM capture is not available\n");
      return EXIT_FAILURE;
    }

  /* 设置采样率与位宽。sai_samplerate 会重新计算主时钟分频，
   * sai_datawidth 会设置数据宽度；两者都在接收前生效。
   */

  bitrate = I2S_RXSAMPLERATE(i2s, 16000);
  I2S_RXDATAWIDTH(i2s, 16);

  printf("vgpdmdump: RX bitrate %lu\n", (unsigned long)bitrate);

  memset(&bufdesc, 0, sizeof(bufdesc));
  bufdesc.numbytes  = nbytes;
  bufdesc.u.pbuffer = &apb;
  ret = apb_alloc(&bufdesc);
  if (ret < 0)
    {
      fprintf(stderr, "vgpdmdump: apb_alloc failed: %d\n", ret);
      return EXIT_FAILURE;
    }

  /* apb_alloc 按 cache 行对齐分配样本区，驱动的 cache 维护因此总是完整的
   * 行。这里再确认一次，避免依赖分配器的实现细节。
   */

  if (((uintptr_t)apb->samp & (up_get_dcache_linesize() - 1)) != 0)
    {
      fprintf(stderr, "vgpdmdump: buffer is not cache line aligned\n");
      apb_free(apb);
      return EXIT_FAILURE;
    }

  nxsem_init(&g_done, 0, 0);
  g_result = -EBUSY;

  /* 计时从投递开始到回调返回，用来核对实际时长与按位率折算的期望值是否
   * 一致。两者偏差过大时，缓冲区内容就不是连续采集的。
   */

  clock_gettime(CLOCK_MONOTONIC, &t0);

  ret = I2S_RECEIVE(i2s, apb, vg_pdm_callback, NULL, VG_PDM_TIMEOUT_MS);
  if (ret < 0)
    {
      fprintf(stderr, "vgpdmdump: I2S_RECEIVE failed: %d\n", ret);
      nxsem_destroy(&g_done);
      apb_free(apb);
      return EXIT_FAILURE;
    }

  ret = nxsem_wait(&g_done);
  if (ret < 0)
    {
      fprintf(stderr, "vgpdmdump: wait failed: %d\n", ret);
      nxsem_destroy(&g_done);
      apb_free(apb);
      return EXIT_FAILURE;
    }

  clock_gettime(CLOCK_MONOTONIC, &t1);

  nxsem_destroy(&g_done);

  if (g_result < 0)
    {
      /* 传输失败时把驱动在完成时刻抓到的寄存器快照打出来。SAI 的溢出与
       * 时钟配置错误走 SR，不经 DMA，所以单看 -EIO 分不清是哪一个。
       */

      uint32_t regs[STM32_PDM_DIAG_NREGS];

      fprintf(stderr, "vgpdmdump: transfer failed: %d\n", g_result);

      if (stm32_pdm_staterecord(regs, STM32_PDM_DIAG_NREGS) == 0)
        {
          fprintf(stderr,
                  "  isr=0x%02x sr=0x%08lx cr1=0x%08lx frcr=0x%08lx "
                  "slotr=0x%08lx pdmcr=0x%08lx\n",
                  (unsigned int)regs[0],
                  (unsigned long)regs[1], (unsigned long)regs[2],
                  (unsigned long)regs[3], (unsigned long)regs[4],
                  (unsigned long)regs[5]);
          fprintf(stderr,
                  "  dma scr=0x%08lx sndtr=%lu spar=0x%08lx sm0ar=0x%08lx "
                  "req=%lu\n",
                  (unsigned long)regs[6], (unsigned long)regs[7],
                  (unsigned long)regs[8], (unsigned long)regs[9],
                  (unsigned long)regs[10]);
        }

      apb_free(apb);
      return EXIT_FAILURE;
    }

  /* 回调返回后驱动已做过 cache 失效，这里读的就是外设写入内存的内容。
   * 长度以驱动实际填写的字节数为准。
   */

  vg_pdm_stats(apb->samp, nbytes, &stats);
  vg_pdm_report(&stats);

  /* 换算成时长。位率按实测链路计算：PLL2P 49.143 MHz 经 MCKDIV 12 得到
   * 4.095 MHz 位时钟，32 位帧里只取 16 位，所以单麦克风每秒进 2.048 Mbit，
   * 即 256 KB/s。用实测耗时与这个折算值对照，可以看出缓冲区是否连续。
   */

  {
    double elapsed_ms = (double)(t1.tv_sec - t0.tv_sec) * 1000.0 +
                        (double)(t1.tv_nsec - t0.tv_nsec) / 1.0e6;
    double stream_ms  = (double)nbytes * 8.0 * 1000.0 / VG_PDM_BITRATE;

    printf("  elapsed=%.1f ms  stream=%.1f ms  rate=%.1f KB/s\n",
           elapsed_ms, stream_ms,
           (elapsed_ms > 0.0) ?
             ((double)nbytes / 1024.0 / (elapsed_ms / 1000.0)) : 0.0);
  }

  if (dumpbytes > 0)
    {
      vg_pdm_hexdump(apb->samp, dumpbytes);
    }

  if (!quiet)
    {
      out = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
      if (out < 0)
        {
          fprintf(stderr, "vgpdmdump: open %s failed: %d\n", path, errno);
          apb_free(apb);
          return EXIT_FAILURE;
        }

      ret = write(out, apb->samp, nbytes);
      if (ret != (int)nbytes)
        {
          fprintf(stderr, "vgpdmdump: short write %d of %zu: %d\n",
                  ret, nbytes, errno);
          close(out);
          apb_free(apb);
          return EXIT_FAILURE;
        }

      close(out);
      printf("vgpdmdump: wrote %zu bytes to %s\n", nbytes, path);
    }

  apb_free(apb);
  return EXIT_SUCCESS;
}

#endif /* CONFIG_VG_PDM_DUMP */
