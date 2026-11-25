# F4SRF 重建规划与新会话入口

状态：PLANNING / 决策已记录，尚未开始新生产代码开发。
暂定名称：Fallout 4 Stylized Rendering Framework (F4SRF) / 辐射4风格化渲染框架。
记录日期：2026-11-26。

## 阅读顺序
1. [架构与产品目标](ARCHITECTURE.md)
2. [实施路线与验收门禁](ROADMAP.md)
3. [历史项目交接与迁移](LEGACY-HANDOFF.md)
4. [技术研究索引](RESEARCH.md)

## 已确认的用户决策
- 等当前 ENMS-B4 源码上传并完成独立审查后，将旧 Endfield 项目作为历史项目保留，完整保存 Git 历史、分支和证据。
- 新实现从届时最新、干净的 Fallout 4 Community Shaders 上游开始；不是在旧 Endfield 生产代码上继续重构。旧成果作为证据和设计参考，不自动移植。
- 目标为明日方舟：终末地（Endfield）和少女前线2：追放（Exilium）的风格化渲染复刻/视觉逼近，共享基础能力，支持风格切换。
- 首先解决角色表现，再正式扩展场景；渲染色调、色彩管理、曝光与 tone mapping 从初期纳入设计，不等场景阶段才考虑。
- Native / Endfield / Exilium / Custom 为候选顶层风格；全局预设和角色、场景、全局色调分模块选择为设计目标。具体参数和算法按视觉证据确定。
- 视觉效果优先：Face、Hair、Skin、Cloth/Metal 优先；Eye NPR 暂缓。
- 支持 OG、NG、AE 及上游后续支持的版本；OG 1.10.163 是当前实机参考，不是唯一产品目标。
- 使用 Codex 本地隔离开发、私有 GitHub 独立源码审查、用户实机验证；不默认合并、部署、公开仓库或覆盖原始资源。

## 分支性质与上游记录
本规划分支基于现有私有仓库 main f83edb7ec7927ef3ccc434fd1aee3ab9d951c10c，仅新增文档；其旧生产树不是重建基线。
2026-11-26 查询到上游 northaxosky/fallout4-community-shaders main: e2bf99f632664435d34dd6612cf1f191cadd1405。该对象尚不在私有仓库可访问的 Git 对象中。实际重建时重新 fetch，记录 exact HEAD、子模块、CI 与版本支持；不以此次观察值强制锁定未来基线。

## 新对话交接提示
请阅读本目录四份文档，以其中的已确认决策作为项目连续性依据；明确区分已确认目标、候选设计和尚未验证的事实。先完成旧 B4 的 GitHub 源码审查及技术债务报告，再经授权归档旧项目、建立干净上游工作树。不得因为已有规划就自动开始大规模重构或继承旧生产代码。开发以实际正常渲染的 Off/On 视觉证据作为阶段完成标准。
