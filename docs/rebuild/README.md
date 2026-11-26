# F4SRF 重建规划与新会话入口

状态：CI-01 实机验收通过；CI-02 发布策略待验收，尚未开始新 NPR 生产代码开发。
暂定名称：Fallout 4 Stylized Rendering Framework (F4SRF) / 辐射4风格化渲染框架。
记录日期：2026-11-26。

## 阅读顺序
1. [架构与产品目标](ARCHITECTURE.md)
2. [实施路线与验收门禁](ROADMAP.md)
3. [历史项目交接与迁移](LEGACY-HANDOFF.md)
4. [技术研究索引](RESEARCH.md)
5. [上游复用审查](UPSTREAM-REUSE-AUDIT.md)
6. [历史最终审查](LEGACY-B4-FINAL-AUDIT.md)
7. [开发与发布流程](DEVELOPMENT-WORKFLOW.md)

## 已确认的用户决策
- 旧 Endfield 项目最终源码审查已完成，仓库 Private + Archived。保留精简源码历史与独有源码修改，不继承旧生产代码或完整构建环境。
- 新实现从届时最新、干净的 Fallout 4 Community Shaders 上游开始；不是在旧 Endfield 生产代码上继续重构。旧成果作为证据和设计参考，不自动移植。
- 目标为明日方舟：终末地（Endfield）和少女前线2：追放（Exilium）的风格化渲染复刻/视觉逼近，共享基础能力，支持风格切换。
- 首先解决角色表现，再正式扩展场景；渲染色调、色彩管理、曝光与 tone mapping 从初期纳入设计，不等场景阶段才考虑。
- Native / Endfield / Exilium / Custom 为候选顶层风格；全局预设和角色、场景、全局色调分模块选择为设计目标。具体参数和算法按视觉证据确定。
- 视觉效果优先：Face、Hair、Skin、Cloth/Metal 优先；Eye NPR 暂缓。
- 支持 OG、NG、AE 及上游后续支持的版本；OG 1.10.163 是当前实机参考，不是唯一产品目标。
- 新仓库已公开；ChatGPT / Codex 分工开发与独立审查，GitHub Actions 云端构建，用户实机验证。合并、部署及正式发布必须经用户授权；不覆盖原始资源。

## 分支性质与上游记录
原始规划来自旧项目，当前 F4SRF 从上游 northaxosky/fallout4-community-shaders 的 e2bf99f632664435d34dd6612cf1f191cadd1405 建立干净基线。CI-01 R1 的 PR HEAD 为 38c1ac7bc2e0d982e5c5a8164d2affedcb707b7a，完整 CI 与用户实机验收已通过。后续上游更新单独审查，不将此版本固定为唯一支持目标。

## 新对话交接提示
请按上述顺序阅读文档，区分已确认目标、候选设计和尚未验证的事实。旧 B4 审查与归档已经完成。CI-02 收尾后开展 ARCH-01，沿实际原生描述符、编译配方及生产消费路径调查。不得自动开始大规模重构或继承旧生产代码。开发以实际正常渲染的 Off/On 视觉证据作为阶段完成标准。
