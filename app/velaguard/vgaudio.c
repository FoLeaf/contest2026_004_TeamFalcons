/****************************************************************************
 * app/velaguard/vgaudio.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Bring-up probe for the audio output path.
 *
 * Two questions this answers, in the order they have to be answered:
 *
 *   1. Does the codec answer on I2C, and does it report the ID the driver
 *      expects?  The WM8994 is behind the same I2C4 bus as the touch
 *      controller, so a wrong address or a bus problem shows up here rather
 *      than as silence during playback.
 *
 *   2. Does the SAI block end up programmed the way playback needs it: master
 *      transmitter with its master clock output enabled, the divider that
 *      produces 256 * FS, and the I2S frame geometry the codec decodes.
 *
 * Playback itself is left to NxPlayer, which exercises the standard audio
 * device.  This command only reports state; it never starts a stream.
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include "vg_audio_fixtures.h"

#include <arch/board/board.h>

#ifdef CONFIG_VG_AUDIO_PROBE

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define VG_CODEC_ID_REG    0x00
#define VG_CODEC_ID_VALUE  0x8994

#define VG_SAI_CR1_OFFSET  0x0000

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void vg_audio_usage(void)
{
  printf("Usage: vgaudio [id|reg <hex>|sai|fixtures|txcheck]\n");
  printf("  id          read the codec ID register\n");
  printf("  reg <hex>   read one codec register, e.g. vgaudio reg 0x1c\n");
  printf("  sai         report the SAI2 block A configuration\n");
}

static int vg_audio_id(void)
{
  uint16_t id = 0;
  int ret;

  ret = stm32_audio_codec_readreg(VG_CODEC_ID_REG, &id);
  if (ret < 0)
    {
      printf("vgaudio: codec read failed: %d\n", ret);
      return ret;
    }

  printf("vgaudio: codec id=0x%04x expect=0x%04x %s\n",
         id, VG_CODEC_ID_VALUE, id == VG_CODEC_ID_VALUE ? "OK" : "MISMATCH");

  return id == VG_CODEC_ID_VALUE ? OK : -EIO;
}

static void vg_audio_sai(void)
{
  uint32_t cr1 = 0;
  uint32_t gcr = 0;
  uint32_t mode;
  uint32_t mckdiv;
  static FAR const char *modes[] =
  {
    "master-tx", "master-rx", "slave-tx", "slave-rx"
  };

  if (stm32_audio_sai_state(&cr1, &gcr) < 0)
    {
      printf("vgaudio: SAI2 state unavailable\n");
      return;
    }

  mode    = cr1 & 3;
  mckdiv  = (cr1 >> 20) & 0x3f;

  printf("vgaudio: SAI2_A cr1=0x%08lx gcr=0x%08lx\n",
         (unsigned long)cr1, (unsigned long)gcr);
  printf("vgaudio: mode=%s nodiv=%lu mckdiv=%lu reserved27=%lu saien=%lu\n",
         modes[mode],
         (unsigned long)((cr1 >> 19) & 1),
         (unsigned long)mckdiv,
         (unsigned long)((cr1 >> 27) & 1),
         (unsigned long)((cr1 >> 16) & 1));

  if (mckdiv != 0)
    {
      printf("vgaudio: MCLK = 49142857 / %lu = %lu Hz (target 4096000)\n",
             (unsigned long)mckdiv, (unsigned long)(49142857u / mckdiv));
    }
  {
    uint32_t r[17] = {0};
    if (stm32_audio_tx_record(r, 17) == OK)
      {
        printf("vgaudio: last dma=%lx sr=%lx cr1=%lx frcr=%lx slots=%lx "
               "count=%lu addr=%lx\n", (unsigned long)r[0],
               (unsigned long)r[1], (unsigned long)r[2],
               (unsigned long)r[3], (unsigned long)r[4],
               (unsigned long)r[7], (unsigned long)r[9]);
        printf("vgaudio: dma_done=%lu errors=%lu timeouts=%lu txbytes=%lu\n",
               (unsigned long)r[11], (unsigned long)r[12],
               (unsigned long)r[13], (unsigned long)r[14]);
        printf("vgaudio: sai_underruns=%lu clockerrors=%lu\n",
               (unsigned long)r[15], (unsigned long)r[16]);
      }
  }

}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  if (argc < 2)
    {
      vg_audio_usage();
      return EXIT_FAILURE;
    }

  if (strcmp(argv[1], "fixtures") == 0)
    {
      const unsigned char *data[] = {vg_audio_tone, vg_audio_speech};
      const size_t sizes[] = {sizeof(vg_audio_tone), sizeof(vg_audio_speech)};
      const char *names[] = {"/mnt/emmc/bvo04/tone.wav",
                             "/mnt/emmc/bvo04/speech.wav"};
      int i;
      if (mkdir("/mnt/emmc/bvo04", 0755) < 0 && errno != EEXIST)
        {
          return EXIT_FAILURE;
        }

      for (i = 0; i < 2; i++)
        {
          size_t off = 0;
          int fd = open(names[i], O_WRONLY | O_CREAT | O_TRUNC, 0644);
          if (fd < 0)
            {
              return EXIT_FAILURE;
            }

          while (off < sizes[i])
            {
              ssize_t n = write(fd, data[i] + off, sizes[i] - off);
              if (n <= 0)
                {
                  close(fd);
                  return EXIT_FAILURE;
                }

              off += n;
            }

          if (fsync(fd) < 0 || close(fd) < 0)
            {
              return EXIT_FAILURE;
            }

          printf("vgaudio: fixture %s bytes=%u\n", names[i], (unsigned)off);
        }

      return EXIT_SUCCESS;
    }
  else if (strcmp(argv[1], "txcheck") == 0)
    {
      int r[5] = {0};
      int ret = stm32_audio_tx_check(r, 5);
      printf("vgaudio: txcheck invalid-init=%d zero=%d odd=%d overcapacity=%d "
             "offset=%d rounds=20 result=%d\n", r[0], r[1], r[2], r[3],
             r[4], ret);
      return ret < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
    }
  else if (strcmp(argv[1], "id") == 0)
    {
      return vg_audio_id() < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
    }
  else if (strcmp(argv[1], "sai") == 0)
    {
      vg_audio_sai();
      return EXIT_SUCCESS;
    }
  else if (strcmp(argv[1], "reg") == 0)
    {
      uint16_t val = 0;
      int off;
      int ret;

      if (argc < 3)
        {
          vg_audio_usage();
          return EXIT_FAILURE;
        }

      /* The codec has a 16-bit register map; the higher-numbered registers
       * (0x200 and up) hold the clocking and AIF configuration, so the range
       * has to cover them.
       */

      off = (int)strtol(argv[2], NULL, 0);
      if (off < 0 || off > 0xffff)
        {
          printf("vgaudio: register offset out of range\n");
          return EXIT_FAILURE;
        }

      ret = stm32_audio_codec_readreg((uint16_t)off, &val);
      if (ret < 0)
        {
          printf("vgaudio: reg 0x%02x read failed: %d\n", off, ret);
          return EXIT_FAILURE;
        }

      printf("vgaudio: R%d (0x%02x) = 0x%04x\n", off, off, val);
      return EXIT_SUCCESS;
    }

  vg_audio_usage();
  return EXIT_FAILURE;
}

#endif /* CONFIG_VG_AUDIO_PROBE */
