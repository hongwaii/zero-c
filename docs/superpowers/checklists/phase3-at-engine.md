# Phase 3 — AT 引擎 + DeviceManager 冒烟测试

## 沙箱里可做

- [x] `./build.bat` build 成功
- [x] `./build.bat test` 全部通过
  - [x] test_strbuf 18/18
  - [x] test_json_roundtrip 20/20
  - [x] test_i18n 6/6
  - [x] test_ringbuf 15/15
  - [x] test_ncm_enumerate 0/0
  - [x] test_at_parser 25/25
  - [x] test_at_session 18/18
  - [x] test_diag_state 5/5
- [x] EXE 重新生成

## 真机验证（必须在 Windows 桌面跑）

### 连接 COM 模组

- [ ] 启动 EXE → 切到"多模组"面板
- [ ] 看到 COM 模组一行（label=`COM 7`，状态=DISCONNECTED）
- [ ] 点该行的"连接"按钮
- [ ] 状态从 DISCONNECTED → **READY**，按钮变"断开"
- [ ] stderr 打印 `device_manager: dev N (COM 7) 已连接`
- [ ] 点"断开"按钮 → 状态回 DISCONNECTED
- [ ] stderr 打印 `device_manager: dev N (COM 7) 已断开`

### 状态卡实时（**P3 占位**——diag_service_refresh_now 不等回复，首次进 panel_diag 7 张卡都显示 "-"，等真回复后填值）

- [ ] 切到"现场诊断"面板
- [ ] 7 张状态卡初始可能都显示 `-`（diag 刷新异步）
- [ ] 点"一键健康检查"按钮
- [ ] stderr 打印一串 at_session 收发（at_session 已在后台持续拉）
- [ ] 几秒后 7 张卡的值**部分**填上（拉成功的填，拉失败的保持 -）

### AT 控制台

- [ ] 在 AT 输入框输 `AT`，按回车 / 点"发送"
- [ ] 控制台历史区出现：
  - `> AT`
  - `< OK`
- [ ] 输 `AT+CSQ`
- [ ] 出现：
  - `> AT+CSQ`
  - `< +CSQ: 23,99`（或类似实际值）
  - `< OK`
- [ ] 输 `AT+FOO`
- [ ] 出现：
  - `> AT+FOO`
  - `< ERROR`
- [ ] 输 `AT+CIMI`
- [ ] 出现：
  - `> AT+CIMI`
  - `< 460001234567890`（或类似）
  - `< OK`

### 拔出模组

- [ ] 拔出 COM 模组
- [ ] 切到多模组面板：2 秒内该行消失
- [ ] 切回现场诊断面板：状态卡变 "未连接模组（先在多模组面板点连接）"
- [ ] 切到 AT 控制台：发 `AT` 应回 `(无连接：先在多模组面板点连接)`

### 不在 P3 范围

- ✗ TCP/SSL 拨号上网
- ✗ 发短信模板（按钮显示但点击不响应）
- ✗ 抓 modem log 30s（同上）
- ✗ USB-NCM/RNDIS 通道（spec 接受，P3 暂只支持 com://）
- ✗ 产线测试 / OTA / LLM 集成（spec P4/P5）
- ✗ 真实 diag 值的"自动定期刷新"（按钮触发 only）

## 备注

- P3 是**链路层 + AT 引擎**完整收尾，下一步 P4（诊断服务深化：TCP/SSL/抓 log）
- 任何未通过项需要开 fix task + commit
- 调试时记得 stderr：debug console 用 `set AGENT_DEBUG_CONSOLE=1 && APP.exe`
