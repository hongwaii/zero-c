# CJK 字体放置说明

为了让 Modem Agent 正确显示中文 UI（"现场诊断"、"AT 控制台"等），请把 CJK 字体放在**这个目录下**：

    assets/fonts/cn.otf

## 推荐字体

- 思源黑体 SC（Source Han Sans SC）Regular 子集或完整版
- 其它任何含 GB2312 / 简中常用字 的 TTF/OTF

## 不放会怎样？

EXE 仍能启动，但中文会显示为方块（`□`）。程序不会报错。
启动时如果检测不到 `cn.otf`，会在控制台（如果开了 `AGENT_DEBUG_CONSOLE=1`）打印一行提示。

## v1.0 发布时

会自带一个 ~2–3 MB 的 CJK 子集字体（覆盖 `app/i18n/zh.json` 实际用到的 ~1500 字 + AT log 输出字符），避免用户手动放置。

## 字体文件本身

不在 git 仓库里（被 `.gitignore` 忽略）。本目录只保留这份说明。
