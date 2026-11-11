# C System

[![Build and CLI tests](https://github.com/12313Q2312/C-system/actions/workflows/ci.yml/badge.svg)](https://github.com/12313Q2312/C-system/actions/workflows/ci.yml)

一个用 **C11** 实现的命令行记录管理器。用编号管理工程样件或练习记录，支持新增、查询、修改、删除和列表，数据保存在可直接查看的 TSV 文件里。

## 构建与试用

需要 CMake 3.16+ 和支持 C11 的编译器。在仓库根目录构建：

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

下面以 Windows PowerShell 为例。MinGW 的程序在 `build/c-system.exe`，Visual Studio 生成器在 `build/Release/c-system.exe`。在 Linux 上可用 `./build/c-system`。

```powershell
$exe = '.\build\c-system.exe'
& $exe records.tsv add 101 'Motor bracket'
& $exe records.tsv add 102 'Bearing seat'
& $exe records.tsv update 101 'Motor bracket v2'
& $exe records.tsv get 101
& $exe records.tsv remove 102
& $exe records.tsv list
```

最后只剩 `101    Motor bracket v2`，编号和标题之间为制表符。关闭终端后再次运行 `list`，仍可读到这条记录。若使用同一份样例文件重跑，请换一组编号或新的文件名。

## 数据与测试

- 每行保存一个正整数编号和一个标题。标题长度为 1 至 120 字节，带空格时加引号。
- 写入使用同目录的临时文件，完整写入后再替换原文件。重复编号、缺失记录或格式错误会返回错误并保留原文件。
- 若异常退出留下 `.tmp` 文件，先确认没有运行中的写入，再把该临时文件移走并重试。

装有 Python 3 时，CMake 会注册命令行测试。测试在独立目录创建样例记录，验证跨进程保存、无效输入、原文件保护和并发写入防护。

```sh
cmake -E chdir build ctest -C Release --output-on-failure
```

[C11 实现](src/main.c) · [命令行测试](tests/test_cli.py)
