# Phase 1 — 应用窗壳冒烟测试清单

每次重新构建 Phase 1 之后跑一遍。**所有项必须通过**。

## 构建验证（沙箱里可做）

- [x] `./build.bat` build 成功
- [x] `./build.bat test` 编译 + 运行 3 个 test 可执行文件
  - [x] `test_strbuf.exe` → 18/18 pass
  - [x] `test_json_roundtrip.exe` → 20/20 pass（3 次连跑幂等）
  - [x] `test_i18n.exe` → 6/6 pass
- [x] EXE 重新生成在 `out/APP/APP-1.*.exe`（约 1.69 MB）

## 真机验证（必须在 Windows 桌面跑，**沙箱里做不了**）

启动 EXE（从 `out/APP/` 拷出来或者直接用 build 出的那个）：

- [ ] 窗口是深色工程蓝主题，1280×800，标题 "Modem Agent"
- [ ] 左侧 200px 导航条：5 项 + "AI 助手" 按钮
  - [ ] 现场诊断 / 多模组 / 产线测试 / OTA 升级 / 设置 / AI 助手
- [ ] 启动后默认进入"现场诊断"面板
- [ ] 点击"现场诊断"：左半 AT 控制台（4 行假历史 + 输入框 + 发送按钮），右半 7 张状态卡 + 5 个动作按钮
  - [ ] CSQ / 驻网状态 / 运营商 / 网络制式 / IMEI / IMSI / ICCID
  - [ ] 拨号测试 / 断开 / 抓 log 30s / 发短信模板 / 一键健康检查
- [ ] 点击"多模组"：6 列表格（名称/端口/IP/信号/状态/最后在线），3 行假数据
- [ ] 点击"产线测试"：灰色文字 "产线自动化测试 — v1.1 推出"
- [ ] 点击"OTA 升级"：灰色文字 "OTA 远程管理 — v1.2 推出"
- [ ] 点击"设置"：
  - [ ] 主题单选：工程蓝可选，浅色灰
  - [ ] 语言单选：中文可选，English 灰
  - [ ] LLM 接入：5 个 provider 列出（DeepSeek / Qwen / GLM / Kimi / MiniMax）
  - [ ] 每个 provider 有 name / Endpoint / 默认模型（只读）+ API Key（Password 隐藏）
  - [ ] 保存按钮：点击控制台（如果开了 debug console）打印 "LLM provider 配置已暂存（in-memory，P6 落盘）"
  - [ ] 底部显示版本号
- [ ] 点击"AI 助手"：右侧抽屉弹出
  - [ ] 顶部 disclaimer
  - [ ] 状态行：`LLM providers: 0 / 5 configured`（未填 key 时）
  - [ ] 输入框 + 发送按钮
  - [ ] 输入文字点发送：对话区追加 "User: xxx / AI: (mock) 已收到你的问题：xxx"
- [ ] 关闭 GUI 窗口（X）：进程退出，无崩溃对话框
- [ ] 60 秒内点遍所有面板 + LLM 抽屉，无 crash / assert / 异常

## Debug Console 开关验证

- [ ] 直接双击 EXE：**无**额外 console 窗口弹出
- [ ] `set AGENT_DEBUG_CONSOLE=1 && APP.exe`：多弹一个 console 窗口，关闭 GUI 后 console 里看到 `=== APP-1.1.0... === exit 0`
- [ ] `APP.exe --debug-console`：跟环境变量一样效果

## 中文显示验证

- [ ] 如果**没有** `assets/fonts/cn.otf`：所有中文显示为方块（□）—— 这是预期，P1 阶段需要用户手动放字体
- [ ] 如果**放了一个含 CJK 字形的 TTF/OTF** 在 `assets/fonts/cn.otf`：中文正常显示
- [ ] 启动 EXE 时 stderr 输出一行 `theme_load_fonts: 未找到 assets/fonts/cn.otf`（如果没字体）

## 备注

- 沙箱里只跑 build + 单元测试。**所有"真机验证"项必须在用户的 Windows 桌面跑**。
- 标 DEFERRED 的项需要 Phase 1 完整收尾时在真机上一项项过。
- 任何未通过项都需要开 fix task + commit，不能直接忽略。
