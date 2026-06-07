# Phase 6 — 持久化 + 报告 + 打包 冒烟测试

## 沙箱里可做
- [ ] `./build.bat` build 成功（含 storage 库）
- [ ] `./build.bat test` 全部通过：
  - [ ] test_sqlite_db
  - [ ] test_at_log_writer
  - [ ] test_diag_snapshot_writer
  - [ ] test_llm_chat_writer
  - [ ] test_report_html
  - [ ] test_config_io
  - [ ] P0-P5 测试不回归
- [ ] 真 SQLite 模式下：所有 storage 测试能跑（不是 skip）
- [ ] `./build.bat package`：iscc 找不到时友好提示；找到时 `dist/agent-setup-x.y.z.exe` 生成

## 真机验证
- [ ] 启动 EXE → data/agent.db 自动创建（WAL 文件 data/agent.db-wal / -shm 也存在）
- [ ] 切到"多模组"面板 → 连一台模组 → 切到"现场诊断" → 发几条 AT → 切到"设置" → 点"导出报告" → logs/report.html 出现
- [ ] 打开 report.html（双击或拖到浏览器）：
  - [ ] 显示"Modem Agent Report"标题
  - [ ] Devices 表列出当前连的模组
  - [ ] AT Log 表列出最近 50 条收发记录
- [ ] 关 EXE → data/agent.db 落盘完整（size > 0）
- [ ] 故意删 data/agent.db 后重启 EXE → EXE 重建（不崩）
- [ ] 故意 chmod data/ 不可写（Linux 行为，Windows 跳过）→ EXE 写库失败 → 控制台 "DB disabled — UI in read-only mode"
- [ ] 装 Inno Setup 6 → ./build.bat package → dist/agent-setup-1.1.0.exe 生成
- [ ] 双击 dist/agent-setup-1.1.0.exe → 安装向导 → 完成 → 桌面图标 → 双击启动 → EXE 跑起来

## 不在 P6 范围
- ✗ PDF 报告（v1.0 只 HTML，PDF 留给 v1.1）
- ✗ 代码签名（spec v1.0 明确不做）
- ✗ 自动更新（v1.1+）
- ✗ 多用户 / 多设备 license 管理
- ✗ LLM chat history UI（v1.0 仅写库，不读库展示；v1.1 加）
