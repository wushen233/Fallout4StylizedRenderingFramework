# 历史项目交接

## 已知阶段与证据边界
现有私有仓库：wushen233/Fallout4CommunityShaders-Endfield。
旧 main（规划时远端）：f83edb7ec7927ef3ccc434fd1aee3ab9d951c10c。
ENMS B1 离线 Validator 末次：1054517e388c3e66472d97d1c2d849aa90e3e37e。
Runtime Declaration Probe R1：a6663fa4dde15fcf90e43a07d08e57f15e6e6313。
B2：c1265a97d1424a196cf5339435d196e88a9248ae。OG 正向像素验收，Shape-scoped ENMS.Profile=eye.eyeball 读取，Immediate/Command-Recorded CPU 记录，黄色 GPU Debug。不能外推所有运行时/复杂路径/生产跨阶段能力。
B3A：420e9a21efb06602ade6f854a65b2ef6a6b71fce。
B3A R1：2a3eaf7f3a9ec987f80b73c72cc1004dde5630e8。生产 Eye NPR 视觉效果未确认；合法声明黄色、Actor Coverage 绿色并不证明 Owner=255 或真实高光贡献。用户决定暂缓。
B4：Codex 正在将复现问题的开发分支上传私有仓库，最终 HEAD 尚未获得。用户实机截图显示 Face SDF Debug 绿色/紫色分区，但正常 Hybrid 画面 On/Off 无明显变化。当前原因未确定。
旧基线静态证据：DFLight 多个太阳光照路径重新计算 N·L 第一/第二阴影及 Refined Bands；DFComposite 也存在按法线重算。可能造成新材质决策覆盖或稀释，必须以 B4 exact HEAD 确认，不能将假设写成根因。

## 保留经验，不自动移植
- NIF Shape Extra Data 真实加载、CommonLibF4 读取、Outfit Studio MCP；Shinano 正确资源覆盖路径 Meshes/Avatars/Shinano/Eyes/Eye.nif（不是 Meshes2）。
- B1 Validator、明确的角色/资源契约和安全回退；审查后重用思想/测试。
- B2 复用原生 per-draw lane 的成功经验；原 Lane V2 四舍五入旧值只证明 Role 0–7 安全。不是通用生产 ABI，也不要求新项目继承。
- Actor Ownership 与 OG RT7/t70、Immediate/Command-Recorded 冻结证据按新上游重新评估。
- B3A 的 b7 追加、反射测试、生产 marker 252/253 的碰撞及动态激活问题是经验教训；不自动带回 marker 或共享协议。
- B4 的 Face SDF 作者图、UV、头骨方向等资源与算法研究保留，生产结果尚未验收。
- 不默认继承全局双层阴影、Refined Bands、全局 Toon Takeover、通用高光/边缘光、旧生产 RT7/t70、旧诊断签名；每项在新目标下独立审查。
- 原有记录 D:/UserData/Endfield-recovery/ 下各阶段报告、NIF 和哈希由本地保留；这里是索引而非实际文件备份。不要上传私有模型、游戏资产、凭据或无关二进制。

## 旧项目最终审查交付
GitHub exact HEAD / base / PR、实机 DLL 身份、复现条件、完整生产路径及关键函数、确定根因与反例（或明确未决）、哪些是设计债务/哪些是单纯实现缺陷、迁移/不迁移清单。保留旧分支和历史。正式归档或新仓库建立在审查完成后另行执行；本规划分支不改变旧项目状态。
