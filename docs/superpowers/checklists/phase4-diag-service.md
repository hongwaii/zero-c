# Phase 4 — 诊断服务深化 + Log 冒烟测试

## 沙箱里可做
- [ ] `./build.bat` build 成功
- [ ] `./build.bat test` 全部通过：
  - [ ] test_diag_state_ext N/N
  - [ ] test_diag_ping TCP/UDP 通过
  - [ ] test_diag_ssl 联网 200 或 skipped
  - [ ] test_diag_sms mock 走通
  - [ ] test_diag_log zip 非空
  - [ ] test_diag_health JSON schema 完整

## 真机验证（必须有真模组 + 真网络）
- [ ] EXE 启动后 logs/ 目录被创建
- [ ] 现场诊断面板：底部 5 按钮点击有响应（不再无反应）
  - [ ] 拨号测试：ATD 命令发出，stderr 看到 chan send
  - [ ] 断开：ATH 命令发出
  - [ ] 抓 log 30s：30 秒后 logs/capture.zip 出现且非空
  - [ ] 发短信模板：填号码 + 文本 → 发送 → 几秒后 UI 显示 status=OK 或 +CMS ERROR
  - [ ] 一键健康检查：5-10 秒后 logs/health.json 出现
- [ ] 网络探活区：
  - [ ] TCP ping 1.2.3.4:80 → 显示 FAIL（外网不可达）
  - [ ] TCP ping 127.0.0.1:某端口 → OK + RTT
  - [ ] SSL probe https://www.baidu.com → status=200 + issuer='CN=...'
  - [ ] SSL probe 非法 URL → status=0 + err='...'
- [ ] 拔模组：所有 diag 操作显示 "无连接"
