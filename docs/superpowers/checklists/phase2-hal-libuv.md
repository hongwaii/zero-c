# Phase 2 — HAL + libuv 冒烟测试

## 沙箱里可做

- [x] `./build.bat` build 成功（链接 ringbuf / ncm_chan / device_manager / serial_chan / libuv）
- [x] `./build.bat test` 编译 + 运行所有 test
  - [x] test_strbuf 18/18
  - [x] test_json_roundtrip 20/20
  - [x] test_i18n 6/6
  - [x] **test_ringbuf** 15/15
  - [x] **test_ncm_enumerate** 0 个命中（开发机无模组，预期）
- [x] EXE 重新生成

## 真机验证（必须在 Windows 桌面跑）

### 启动 / stderr 验证

- [ ] 启动 EXE（不带 `AGENT_DEBUG_CONSOLE=1`）：无额外 console 窗口弹出
- [ ] stderr（或 debug console）看到一行：`device_manager: 启动扫描，间隔 500 ms`

### 未插任何模组

- [ ] 切到"多模组"面板
- [ ] 表格区域显示 `暂未发现模组——插上 COM 或 USB-NCM 模组等待 0.5 秒`
- [ ] 每 0.5s stderr 打印 `device_manager: 扫描 diff — COM +0/-0, NCM +0/-0 → 共 0 设备`

### 插上 COM 模组

- [ ] 插上后 ≤ 0.5s，表格自动出现一行：
  - 名称 = `COM 7`（或实际端口号）
  - 端口/URI 列 = `com://COM7?baud=115200`
  - IP 列 = `-`
  - 信号 = `0`
  - 状态 = `DISCONNECTED`
  - 最后在线 = `-`
- [ ] stderr 打印 `device_manager: 扫描 diff — COM +1/-0, NCM +0/-0 → 共 1 设备`
- [ ] 拔出后 ≤ 0.5s，该行消失
- [ ] stderr 打印 `device_manager: 扫描 diff — COM +0/-1, NCM +0/-0 → 共 0 设备`

### 插上 USB-NCM 模组（如 Air724 / EC200N 等）

- [ ] 插上后 ≤ 0.5s，表格自动出现一行：
  - 名称含 `RNDIS` / `Mobile` / `NCM` 关键字
  - 端口/URI 列 = `rndis://<名字>`
  - IP 列 = 非空 IPv4（如 `10.42.0.7`）
- [ ] stderr 打印 NCM +1 提示

### Debug console 开关

- [ ] `set AGENT_DEBUG_CONSOLE=1 && APP.exe`：console 弹出，所有 `device_manager:` / `serial_chan:` / `ncm_enumerate:` 日志可见

### 其它面板不受影响

- [ ] 切回"现场诊断"面板：AT 控制台 + 7 张状态卡 + 5 个动作按钮**仍然正常**（P2 改的 panel_devices 之外没破坏）
- [ ] 切到"设置"面板：5 个 LLM provider + 主题单选 + 语言单选 + 保存按钮**仍然正常**
- [ ] 切到"AI 助手"抽屉：能开能关、对话区、发送按钮**仍然正常**

## 不在 P2 范围

- ✗ 手动点连接 → 真发 AT → 看回码（这是 **P3** 的事）
- ✗ 模组状态从 DISCONNECTED 切到 READY（也是 P3）
- ✗ IMEI/IMSI/ICCID 等真实值（AT 引擎解析，P3 之后）

## 备注

- 沙箱只跑 build + 单元测试。所有"真机验证"项需要用户的 Windows 桌面 + 真模组
- 任何未通过项需要开 fix task + commit
