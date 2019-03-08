# 尝试c++嵌入c#(基于coreclr,macos平台开发)

- mac下安装dotnet
  - brew update
  - brew tap caskroom/cask
  - brew cask install dotnet

- 编译测试运行
  - 编译: ./bin/build.sh
  - 运行: ./host /usr/local/share/dotnet/shared/Microsoft.NETCore.App/2.0.0/

- 问题:
  - 只有OutputType为Exe模式,并且netcoreapp为3.0才能正常运行,这样会拷贝所有dll到生成目录,其他都不会拷贝,运行时会报错

- 参考:
  - https://yizhang82.dev/hosting-coreclr
  - https://github.com/dotnet/docs/blob/master/docs/core/tutorials/netcore-hosting.md
  - https://github.com/dotnet/samples/tree/master/core/hosting/HostWithCoreClrHost