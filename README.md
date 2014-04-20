ffengine
========

ffengine是高效的tcp 服务器框架，适用于端游、页游、手游的实时服务器

## FFRPC Feature
 *  组件化，ffengine 包含两个重要组件gate和scene，但是集成在一个进程中，既可以把两个组件启动到一个进程中，也可以启动到多个进程中，甚至启动到分布式环境，只需要修改一下ip，这得益于ffengine使用了原创的ffrpc通信库，使得进程间通讯非常的高效和简洁。
 *  
