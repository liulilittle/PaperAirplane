# PaperAirplane
<br />
  一个类似于Proxifier的SOCKS5代理工具，它本身是基于supersocksr的一个子项目；主要是为了解决supersocksr全局应用层代理的一个问题；在src文件夹内各个不同的文件对应着，PaperAirplane的一个实现方式与思路；每种思路有好处有坏处，总之就本人推荐的话还是“relay server”的会好狠多；“connect”、“send”方式的只能chrome、firebox代理上网。
<br />
  installer的代码主要适用于relay server；不过你可以简单修改一下installer目录中的lsp.cpp文件内的ProviderId为connect、send的代码内的ProviderId就可以了；
<br />
实现方式：
connect         从链接地址上实现
<br />
send            从发送数据上实现
<br />
relay server    从中继服务器实现
<br />
如何编译：
<br />
  对于编译的话比较简单；直接用vs 2013,2015,2017(x86 and x64)打开src源代码直接build就行；
<br />
如何使用：
<br />
  默认劫持应用层链接至127.0.0.1:1080的socks5代理服务器上；不过建议的话还是简单的修改修改是不错的、
  
