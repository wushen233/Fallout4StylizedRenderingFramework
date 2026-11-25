# 上游复用审查 — F4SRF

日期：2026-11-26。上游 northaxosky/fallout4-community-shaders，main exact HEAD `e2bf99f632664435d34dd6612cf1f191cadd1405`。这是调查快照，不是未来重建的强制锁定版本。已阅读该提交 README、docs/FEATURES.md、VanillaPBR、SubsurfaceScattering、IBL、HDRDisplay、RemoteControl deviation records，并核查 features/ 目录。上游 Beta；Feature 默认 inactive；存在源码不代表已完整移植或实机验证。

## 项目边界
用户决定新建独立仓库（建议 Private，名称 Fallout4StylizedRenderingFramework，待正式创建）；旧仓库待 B4 最终审查、保存本地与远端证据后归档只读。新仓库保留上游 Git 历史与许可证，独立 origin/upstream，迁入 docs/rebuild/ 规划文档而非旧生产提交。F4SRF = 上游原有写实/现代渲染能力 + NPR 材质模型、资产契约、风格配置与必要配套。优先补充，而不是替换原有渲染器。正式开发开始前重新获取最新上游和子模块。

## 复用矩阵
| 能力 | 上游现状 | F4SRF 处理 |
|---|---|---|
| Vanilla PBR | 已有目录，Pt；FO4 原有 gloss/强度映射、GGX、环境反射；无通用 metalness 输入 | 保留基础，按材质调整漫反射/高光，不声称等于 TruePBR |
| SSS | Pt，原生 class 5，Human/Base profile，Separable/Burley，替换原生皮肤模糊；首人称例外 | 保留并验证与 Face/Skin NPR 的组合，避免二次散射或冲淡 SDF |
| Dynamic Cubemaps / IBL / Skylighting / SSGI | 已有功能；IBL 依赖 Dynamic Cubemaps，ambient 在 deferred light 中替换，deferred specular IBL 不直接应用 | 复用环境光、间接光、反射；按材质控制贡献，保留真实遮挡 |
| Screen Space / Terrain / Cloud / Volumetric Shadows | 已有 Feature，部分移植 | 保留几何遮挡；Face SDF 是艺术明暗边界，不能代替场景遮挡 |
| Light Limit Fix / Sky Sync | 已有 Feature | 保留局部光和太阳/月亮方向；不重写光源体系 |
| Extended Materials | Pt，POM、视差阴影、地形；不是任意 NPR 纹理绑定机制 | 参考现有资源工作流和 deferred 传输设计，独立 NPR 资源契约 |
| Terrain Variation/Blending/Shadows, LOD Blending, Water Effects/Unified Water | 已有目录 | 场景阶段优先复用 |
| HDR Display | Pt，HDR10 输出、Hable tonemap 扩展、原生 3D LUT 在 tonemap 后，UI 分离及 HDR 输出兼容 | 从实际 SDR/HDR 色彩管线出发，避免重复 gamma/tonemap/LUT；不把 HDR 输出等同于完整自定义调色 |
| Upscaling / Frame Generation / Motion Vector Fixes | 已有目录 | 保持兼容，避免破坏时域数据 |
| CSEditor / Screenshot / RenderDoc / PerformanceOverlay / RemoteControl | 已有目录；RemoteControl 需 DevBench，支持 Feature、实时设置、Debug、截图、捕获及性能比较接口 | 优先调查和实机验证；作为自动化视觉 QA 的候选工具链 |

## 上游尚未完成的关键能力
FEATURES.md 标记 NS（not started）：Hair Specular、Skin、TruePBR、Effects11、Post Processing、Exponential Height Fog、Physical Sky 等。不能当成现成能力，也不提前全量移植；跟踪上游，按角色优先的实际缺口实现最小功能。原生 FO4 已是线性光照；不要再造无必要的全局 linearization。
- NPR Face SDF、角色材质身份/作者资源、专用 Hair Ramp/方向高光、Skin NPR、Cloth/Metal NPR 响应、角色专用阴影/描边、Eye NPR 等为 F4SRF 重点。
- 上游已存在眼睛 spare material class 和 hair 特殊 G-buffer 数据；严禁默认空闲编码或统一覆盖。
- SSS 与 IBL 各有既有 deferred consumer；以实测决定协同，不因视觉问题直接禁用上游 Feature。
- 色调系统分材质、光照、图像三级，优先复用 Hable/原生 LUT 与 HDR 设计；Effects11/完整自定义后处理当前未完成，避免与上游正在进行的移植重复。

## 新仓库建立后的第一项工作
对干净上游编译并建立 Native baseline（逐 Feature 激活状态、可重复场景、曝光、角色/场景截图、帧时间）。研究 CSEditor 锁天气/时间及 DevBench RemoteControl/Screenshot/RenderDoc/PerformanceOverlay 的实际可用性。再以最小 Face NPR 生产垂直切片验证完整数据消费与视觉效果，不先实现大型框架或继承旧阴影债务。保留 Endfield/Exilium/Native/Custom 风格架构和角色优先、色彩贯穿、场景后续的已确认目标。

## 来源（固定提交）
- https://github.com/northaxosky/fallout4-community-shaders/blob/e2bf99f632664435d34dd6612cf1f191cadd1405/README.md
- https://github.com/northaxosky/fallout4-community-shaders/blob/e2bf99f632664435d34dd6612cf1f191cadd1405/docs/FEATURES.md
- https://github.com/northaxosky/fallout4-community-shaders/tree/e2bf99f632664435d34dd6612cf1f191cadd1405/docs/deviations
