# 尝试c++嵌入c#(基于coreclr,macos平台开发)

- mac下安装dotnet
  - brew update
  - brew tap caskroom/cask
  - brew cask install dotnet

- 编译测试运行
  - 编译: ./bin/build.sh
  - 运行: ./host /usr/local/share/dotnet/shared/Microsoft.NETCore.App/2.0.0/

- 问题:
  - .csproj中选择netcoreapp2.0以上可以正常测试，但是会拷贝很多dll到bin目录下
  - .csproj中选择netcoreapp2.0运行时会报错,但是只会生成一个ManagedLibrary.dll

- 参考:
  - https://yizhang82.dev/hosting-coreclr
  - https://github.com/dotnet/docs/blob/master/docs/core/tutorials/netcore-hosting.md
  - https://github.com/dotnet/samples/tree/master/core/hosting/HostWithCoreClrHost