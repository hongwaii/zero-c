# CJK 字体放置说明

Modem Agent 启动时按以下顺序**自动**加载 CJK 字体：

1. **`assets/fonts/cn.otf` 或 `assets/fonts/cn.ttf`**（用户自放 / v1.0 打包自带）
2. **Windows 系统已装的中文字体**（按 `C:\Windows\Fonts\` 下的已知文件路径依次尝试）：
   - `msyh.ttc`（微软雅黑，TTC，FontNo=0 取 regular）
   - `msyhbd.ttc`（微软雅黑粗体）
   - `msyhl.ttc`（微软雅黑细体）
   - `simhei.ttf`（黑体）
   - `simsun.ttc`（宋体）
   - `simfang.ttf`（仿宋）
   - `Deng.ttf`（等线）
   - `SourceHanSansSC-Regular.otf`、`NotoSansCJKsc-Regular.otf`（如果用户主动装过）
3. ImGui 默认字体（仅当以上全失败；中文会显示为方块，ASCII 正常）

## 为什么不直接用 Windows API (GetFontData) 取系统字体？

早期实现用 `GetFontData` 拿 CJK 字体数据再用 `AddFontFromMemoryTTF` 加载。
但 Windows 系统字体（YaHei/SimSun 等）大多是 **OTF/CFF 或 TTC** 格式，
而我们用的 ImGui 1.92.9 只有 `AddFontFromMemoryTTF`（不支持 OTF/CFF）。
TTF parser 静默失败产生"0 glyph 的空壳"字体，导致整片空白。

走文件路径 + `AddFontFromFileTTF` 是最稳的方案（该 API 内部自动检测 TTF/OTF/TTC）。

## v1.0 发布时

会自带一个 ~2–3 MB 的 CJK 子集字体（覆盖 `app/i18n/zh.json` 实际用到的 ~1500 字 + AT log 输出字符），
放在 `assets/fonts/cn.otf` 或打包进 EXE 资源段。
