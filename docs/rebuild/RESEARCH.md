# 技术研究索引

以下为此前对话中收集的候选参考入口，不等于全部已核验的原版实现。正式实施时重新检查来源、版本、许可证、可复现性；区分官方开发者说明、社区分析、仿制项目、自己的推断。不复制专有游戏资源，不以第三方复刻证明官方精确公式。

## Endfield
- https://github.com/Morgana-lgtm/Arknights-Endfield-Inspired-Perlica-Character-Shader
- https://github.com/congyuxiaoyoudao/Endfield_Character_Rendering
- https://github.com/blurfeng/arknights-endfield-npr
- https://zhuanlan.zhihu.com/p/2078163827716375618 （西池，UE5 仿终末地 Face SDF 贴图制作与使用；已阅读正文）
- 既有 ENMS-A/B0/B1、EndfieldRace 研究记录（本地）。独立渲染器案例不能直接证明 FO4 原生生产路径；尊重作者许可。

## Exilium / 少前2
- https://cg-x.com/1565.html （此前收集的维普蕾角色渲染分析入口）
- https://www.gcores.com/articles/192957 （制作团队采访候选）
- https://game.xiaomi.com/viewpoint/1392697565_1644457607820_11 （美术团队观点候选）
- https://fenglvv.github.io/pages/3r2_cn.html （Toon Shader 技术参考）
- https://github.com/ColinLeung-NiloCat/UnityURPToonLitShaderExample

## 作者工具
- https://github.com/Yu-ki016/SDFTool
- https://github.com/fenglyu1314/MyaSDFTools
- https://github.com/ousnius/BodySlide-and-Outfit-Studio/wiki/MCP-Server-for-AI-agents
Face SDF 使用多角度二值阴影图、距离变换和插值生成方向阈值图；使用实际头部坐标系并匹配生成/运行角度编码。核验 UV、纹理线性色彩空间、工具输出精度和许可证。工具候选不等于已安装或运行成功。

## 研究重点
建立两款游戏技术矩阵，记录共同算法、特有模型/资源、证据等级、目标视觉样例、实现阶段和 FO4 管线差异。重点包括面部 SDF、头发 Ramp/高光、皮肤、服装金属、刘海、自阴影、描边、眼部、透明、色彩管理与场景。视觉参考需控制场景和光照差异；不预设未经测量的色调或精确参数。上游：https://github.com/northaxosky/fallout4-community-shaders ；开始重建时重查最新源码及 CI。
