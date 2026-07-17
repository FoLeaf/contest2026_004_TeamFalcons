# AI Bridge Stub（主机侧，无板也可测）

在 PC 上模拟「订阅诊断请求 → 回假 AI 响应」，用于无 RJ45 时验证 MQTT 合同。

## 依赖

```bash
# Debian/Ubuntu 示例
sudo apt-get install -y mosquitto mosquitto-clients python3-paho-mqtt
# 或
pip3 install paho-mqtt
```

本地 broker 监听 `1883`（仅试验网）。

## 启动 stub

```bash
cd tools/ai_bridge_stub
python3 bridge_stub.py --host 127.0.0.1 --port 1883
```

## 模拟板端发诊断请求

```bash
DEVICE=vg-test-001
REQ=req-demo-1
mosquitto_pub -h 127.0.0.1 -t "vg/${DEVICE}/ai/request" -q 1 -m "{
  \"req_id\": \"${REQ}\",
  \"type\": \"diagnosis\",
  \"device_id\": \"${DEVICE}\",
  \"alarm\": {\"level\": \"WARNING\", \"summary\": \"温度偏高\", \"temp_c\": 72.5, \"threshold_c\": 70}
}"
```

另开终端订阅响应：

```bash
mosquitto_sub -h 127.0.0.1 -t "vg/+/ai/response/#" -v
```

应看到 `vg/vg-test-001/ai/response/req-demo-1` 与 JSON 假诊断。

## 说明

- 不调用真实 MiMo；仅返回固定结构，便于板端解析器联调。
- 订阅：`vg/+/ai/request`；发布：`vg/{device_id}/ai/response/{req_id}`（`req_id` 与 JSON 内字段一致）。
- 响应字段：`ok`、`summary`、`risk_level`、`possible_causes`、`recommended_actions`、`need_shutdown`、`confidence`（另带 stub 标记 `source`）。
- 有网线后：broker 可改局域网 IP，板连同一 broker；stub 可继续当假云。
- 合同全文：`docs/velaguard-mqtt-contract.md`。
