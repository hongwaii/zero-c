# Phase 5 — LLM 客户端冒烟测试

## 沙箱里可做
- [ ] `./build.bat` build 成功
- [ ] `./build.bat test` 全部通过：
  - [ ] test_llm_sse 5/5
  - [ ] test_llm_dpapi 2/2
  - [ ] test_llm_client_mock tokens='Hello world' ok=1
  - [ ] test_llm_tool 2/2
  - [ ] test_provider_config roundtrip OK
  - [ ] 老的 P0-P4 测试不回归

## 真机验证（必须有真实 LLM API key）
- [ ] 启动 EXE：自动从 config/llm_providers.json 加载 5 厂商
  - [ ] 若文件不存在：自动 seed 5 厂商 + 落盘
- [ ] 切到"设置"页：5 厂商列出，每个可改 key
  - [ ] 填 DeepSeek key → 点"保存" → 关闭 EXE → 重启 → key 仍在
  - [ ] 看 config/llm_providers.json：api_key 是 hex 字符串（明文 DPAPI 加密后）
- [ ] 切到"AI 助手"抽屉
  - [ ] 顶部状态行：`LLM providers: 1 / 5 configured`（填了几个就显示几）
  - [ ] 输"你好" → 点发送
  - [ ] 流式 token 逐字显示（不是等全部到齐再显示）
  - [ ] 几秒后 AI 完整回复出现
  - [ ] status 区域无 401 / timeout
- [ ] 故意填错 key（截断最后 5 字符）：
  - [ ] 重启 EXE → 发请求 → 几秒后红色 status: HTTP 401
  - [ ] 修正 key → 重启 → 重发 → 正常回复
- [ ] 故意拔网线（或者断 WiFi）：
  - [ ] 发请求 → 几秒后 status: curl: Failed to connect
- [ ] 拔模组（COM）：
  - [ ] LLM 抽屉仍正常工作（LLM 不依赖模组）
- [ ] tool_call 弹窗（v1.0 不真发）：
  - [ ] 输入"帮我查 CSQ"→ 等 AI 响应 → 若 AI 返回 tool_call → 弹模态
  - [ ] 模态显示 function name + arguments（arguments 是只读）
  - [ ] 点"确认并执行"：history 追加一行 "[tool] would execute: send_at({...})"
  - [ ] 点"取消"：模态关闭，history 无追加

## 不在 P5 范围
- ✗ 真正的 tool 执行（v1.1）
- ✗ 多轮对话上下文（v1.0 只发当次 input）
- ✗ tool_call 流式解析（v1.0 等完整响应后再解析）
- ✗ SSE 失败重连（v1.0 一次失败就报）
- ✗ 中文 / 英文双语 UI（spec v1.1）
