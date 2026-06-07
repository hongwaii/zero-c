# CJK 字体放置说明

Modem Agent 启动时会**自动按以下三级顺序**加载 CJK 字体：

1. **`assets/fonts/cn.otf`**（用户自放 / v1.0 打包自带）
2. **Windows 系统已装的中文字体**（微软雅黑 UI / 微软雅黑 / 宋体 / 黑体 / 等线 / 思源黑体 / Noto Sans CJK SC / Noto Sans SC）
3. **ImGui 默认字体**（中文会显示为方块，仅在前两步都失败时）

## 不放会怎样？

不需手动放！EXE 启动时如果没找到 `assets/fonts/cn.otf`，会自动用 Windows 系统的中文字体（微软雅黑等）。

只有当**两步都失败**（极少见：系统连微软雅黑都没装，比如极简 Linux 子系统、特殊 Windows 容器），中文才会显示为方块。stderr 也会打印警告。

## v1.0 发布时

会自带一个 ~2–3 MB 的 CJK 子集字体（覆盖 `app/i18n/zh.json` 实际用到的 ~1500 字 + AT log 输出字符），避免对系统字体的依赖。

## 字体文件本身

不在 git 仓库里（被 `.gitignore` 忽略）。本目录只保留这份说明。
