# 架构与产品目标

## 1. 产品定位
F4SRF（暂定）是面向 Fallout 4 的风格化渲染扩展，基于 Community Shaders，不是独立游戏渲染器。第一批参考风格为 Endfield 和 Exilium。最终覆盖角色、场景、色调与后处理。以原创/获准资产和公开技术原理实现视觉逼近，不宣称已获得两款游戏完整专有 Shader 或保证像素级一致。

## 2. 总体设计（候选，待干净上游验证）
- Upstream Integration：上游 Feature 生命周期、Shader Injection、原生 deferred pipeline、版本适配与 ABI。
- Rendering Core：Actor Identity、Material Resolver、Resource Binding、Draw-local/Per-pixel Transport、生命周期、有效性与安全回退。
- Material Models：Face SDF、Hair NPR、Skin/SSS、Cloth/Leather/Metal、Eye、角色阴影、描边、特殊透明/绒毛等。优先共享数学与资源契约，必要时允许风格专用模型。
- Style Profiles：选择各材质模型、纹理/参数/Ramp、光照权重、阴影与色彩响应。Native、Endfield、Exilium、Custom；不将原先全局 Toon/Hybrid 当作产品架构根节点。
- Environment：后续场景材质、地形、植被、水体、天气、天空、间接光和体积效果，优先复用上游。
- Color Pipeline：材质色彩、光照色彩、图像色彩，贯穿全程。
- Composition：统一 HDR 光照、遮挡、反射、曝光、雾效和后处理；避免双重着色或重复 tone mapping。

## 3. 独立身份与职责
Material Role（部位语义）、Material Profile（作者资源/参数）、Material Instance（具体实例）、Shading Model（算法）、Style Profile（模型与参数组合）、Actor Ownership（作用范围）、Render Queue（绘制路径）不可混用。NIF Shape-scoped NiStringExtraData 是已在 OG 验证的作者声明机制，旧 ENMS.Profile 是角色选择器而不是完整 Profile ID；新键名与 schema 尚未冻结。优先明确 opt-in、默认 Native、未知/缺失/冲突/不兼容资源回退；不以 Shinano 名称或纹理文件名作为生产身份。兼容历史声明是否有必要待定。
每类材质只有一个明确的主漫反射/风格阴影决策来源；旧的通用 N·L、双层阴影、Refined Bands 不得重新覆盖 Face SDF 等专用决策。真实几何遮挡、场景阴影、间接光与艺术阴影分开处理。Hybrid/PBR/NPR 权重由模型和风格决定。未适配对象保持 Native，而非默认强制全局 NPR。
生产着色不得依赖诊断开关、日志探针或临时 albedo 标记。

## 4. GPU 数据与资源契约
从最新上游审查实际资源占用、Shader Reflection、宿主分配、绑定、写掩码、清除/混合、采样、原生/注入 Shader 切换和 cache identity。区分 Draw-local 角色、材质参数/纹理与每像素光照决策；按真实消费者需要设计跨阶段数据。Immediate 与 Command-Recorded 必须对应同一 Geometry，录制值正确冻结，动态头骨/参数具有明确刷新策略；不保留未验证的裸指针、不引入无界每 Draw 分配或 GPU 同步等待。独立资源或版本化共享协议均可作为候选，按实证选择；不因不存在现成接口而反复阻断，也不盲目挤入旧 Lane/RT7/t70 或 Material Class ID。明确所有消费者、碰撞、默认值、功能关闭、版本/缓存和混合版本风险。允许合理扩展框架自有 ABI；原生布局修改、新 RT 或共享协议需具体论证。
角色资源与风格切换分离；允许实时切换参数，纹理/变体/录制数据切换须证明生命周期安全，必要时重启或明确刷新。Native 应恢复未启用本框架的上游行为。

## 5. 双风格与材质能力
Face：作者阈值图、真实动画 Head Forward/Right/Up、头部平面光向、左右 UV 采样、阈值/柔化、实际主光和遮挡协调。角度归一化必须匹配生成契约；UV 非对称不得强行镜像；验证相机环绕与头部转动。
Hair：方向性/各向异性高光、Diffuse/Specular Ramp、切线或独立高光法线、发片及刘海阴影，透明路径独立评估。
Skin：稳定肤色、面部身体一致性、柔和阴影、SSS 与物理光照协调。
Cloth/Leather/Metal：分材质漫反射、粗糙度、反射、金属与织物响应，保留质感。
Eye：MatCap、虹膜/角膜、作者高光遮罩、视差等后续；旧 B3A 程序化高光未通过生产视觉验收。
Outline/Character Shadow：角色轮廓、刘海投影、自阴影、透发眉眼等独立能力；验证遮挡和透明合成。
共同技术并不意味着两款游戏具体参数或原版实现相同。先做同一测试资产下可切换的模型与参数，风格特有资源按需提供，不保证任意资产不经适配即可复刻。

## 6. 色彩管理（核心，不是最后加滤镜）
Material Color：线性基础色、材质阴影色、高光色、Ramp、纹理颜色空间；数据纹理（SDF/Mask）不走 sRGB。
Lighting Color：太阳/局部光、天空光、环境/间接光、光照强度、角色与场景统一。
Image Color：HDR 工作空间、曝光、白平衡、tone mapping、highlight roll-off、对比、饱和度、色彩曲线、LUT、Bloom。先查清 FO4 与上游实际管线及 SDR/HDR 输出，避免重复 gamma、曝光或 tone mapping；不以全屏滤镜替代材质光照修复。角色阶段保持中性可重复基准，同时设计和按需验证色调；完整场景风格和全局分级后续实施。
Style Profile 可包含 Character / Environment / Color 子配置；全局预设一键选定三者，允许独立选择和自定义。分模块选择可能形成非目标游戏组合，应明确为自定义。实际色彩曲线与参考素材测量待研究，不能预设两款游戏固定冷暖倾向。

## 7. 性能与兼容
目标版本由上游实际支持范围及用户需求确定（OG、NG、AE、后续最新）；每个版本区分编译/ABI/真实运行证据。保持其他 Community Shaders Feature 和非 opt-in 场景行为。记录 GPU 时间、帧时间分位、VRAM、变体/缓存、资源成本；以真实对照测量确定预算。工具版本遵循项目实际最低/强制锁定，非强制补丁版本差异不单独阻断。
