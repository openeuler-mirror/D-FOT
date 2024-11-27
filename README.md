# D-FOT

#### 介绍
D-FOT是动态反馈优化框架，支持应用无感知反馈优化（启动时优化/运行时优化），当前已实现基于oeAware的启动时优化功能。

#### 软件架构
本仓库包含1个调优插件dfot_tuner_sysboost
1. dfot_tuner_sysboost调优插件基于[sysboost](https://gitee.com/openeuler/sysboost)实现，用于对目标应用实施二进制优化

#### 安装教程

依赖安装：
1. [oeAware-manager](https://gitee.com/openeuler/oeAware-manager)
2. [libkperf](https://gitee.com/openeuler/libkperf)
3. [sysboost](https://gitee.com/openeuler/sysboost)

源码编译：
```shell
git clone https://gitee.com/openEuler/D-FOT.git
mkdir build && cd build
cmake .. -DLIB_KPERF_LIBPATH=/usr/lib64/ -DLIB_KPERF_INCPATH=/usr/include/libkperf/
make
```

#### 使用说明

全局配置文件：`/etc/dfot/dfot.ini`

请查阅[oeAware用户指南](https://gitee.com/openeuler/oeAware-manager/blob/master/docs/oeAware%E7%94%A8%E6%88%B7%E6%8C%87%E5%8D%97.md)，以下给出简单使用方式
```shell
# 插件库加载，或拷贝至/usr/lib64/oeAware-plugin/下默认启动加载
oeawarectl -l libdfot.so
# 插件使能
oeawarectl -e dfot_tuner_sysboost
# 插件去使能
oeawarectl -d dfot_tuner_sysboost
```

#### 约束限制
1. 优化对象必须具有重定位信息

#### 参与贡献

1.  Fork 本仓库
2.  新建 Feat_xxx 分支
3.  提交代码
4.  新建 Pull Request
