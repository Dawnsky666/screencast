#pragma once

namespace sc {

// 无界面自检：采集 -> 编码 -> 解码，跑通整条链路并打印耗时
int runSelfTest(int frames);

} // namespace sc
