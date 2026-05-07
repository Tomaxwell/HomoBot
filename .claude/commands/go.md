# /go — 交付验证

当前工作完成后，执行以下验证闭环，全部通过后输出交付摘要。

## Step 1：编译验证

在工作空间根目录运行：
```
colcon build --cmake-args -DCMAKE_BUILD_TYPE=Release 2>&1
```
- 若有编译错误，停止并报告错误，不继续后续步骤
- 记录编译耗时和 warning 数量

## Step 2：测试验证

```
colcon test --event-handlers console_cohesion+ 2>&1
colcon test-result --verbose 2>&1
```
- 若有测试失败，停止并报告失败用例，不继续后续步骤
- 记录通过/失败/跳过用例数

## Step 3：代码审查

调用 `/review` 命令对本次修改的文件进行审查。
- Critical 或 High 级别问题必须修复后重新执行 /go
- Medium 及以下记录到摘要中

## Step 4：输出交付摘要

以表格形式输出：

| 项目 | 结果 |
|------|------|
| 涉及模块 | （模块名列表） |
| 编译结果 | PASS / FAIL |
| 测试结果 | X passed, Y failed |
| 代码审查 | PASS / X issues |
| 新增接口 | Topic X个 / Service Y个 / Action Z个 |
| 关键变更 | （一句话描述） |

全部通过后提示：**✓ 可以提交**
