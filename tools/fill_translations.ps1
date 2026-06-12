# Fills zh_CN translations into the lupdate-generated .ts file.
$tsPath = "D:\Work\AIAssistent\Dependency\resources\i18n\qdepends_zh_CN.ts"

$dict = @{
    "Ordinal" = "序号"
    "Hint" = "提示值"
    "Function" = "函数"
    "Entry Point" = "入口点"
    "N/A" = "N/A"
    "Module" = "模块"
    "File Time Stamp" = "文件时间戳"
    "Link Time Stamp" = "链接时间戳"
    "File Size" = "文件大小"
    "Attr." = "属性"
    "Link Checksum" = "链接校验和"
    "Real Checksum" = "实际校验和"
    "CPU" = "CPU"
    "Subsystem" = "子系统"
    "Symbols" = "符号"
    "Preferred Base" = "首选基址"
    "Actual Base" = "实际基址"
    "Virtual Size" = "虚拟大小"
    "Load Order" = "加载顺序"
    "File Ver" = "文件版本"
    "Product Ver" = "产品版本"
    "Image Ver" = "映像版本"
    "Linker Ver" = "链接器版本"
    "OS Ver" = "OS 版本"
    "Subsystem Ver" = "子系统版本"
    "Find (functions and modules)" = "查找（函数和模块）"
    "Previous match (Shift+F3)" = "上一个匹配 (Shift+F3)"
    "Next match (F3)" = "下一个匹配 (F3)"
    "Close (Esc)" = "关闭 (Esc)"
    "No results" = "无结果"
    "%1 of %2" = "第 %1 项，共 %2 项"
    "Copy" = "复制"
    "Copy Undecorated Name" = "复制反修饰名称"
    "Disassemble..." = "反汇编..."
    "Disassembly — %1" = "反汇编 — %1"
    "Address" = "地址"
    "Bytes" = "字节"
    "Instruction" = "指令"
    " (truncated)" = "（已截断）"
    "%1   ·   %2   ·   entry 0x%3   ·   %4 instructions%5" = "%1   ·   %2   ·   入口 0x%3   ·   %4 条指令%5"
    "This export is forwarded to %1 and has no local code." = "该导出转发至 %1，没有本地代码。"
    "This export has no entry-point RVA." = "该导出没有入口点 RVA。"
    "Copy All" = "全部复制"
    "Clear Log" = "清空日志"
    "&File" = "文件(&F)"
    "&Open..." = "打开(&O)..."
    "Open PE File" = "打开 PE 文件"
    "PE Files (*.exe *.dll *.sys *.ocx *.cpl *.drv *.scr *.mui *.ax *.efi);;QDepends Session (*.qds);;All Files (*)" = "PE 文件 (*.exe *.dll *.sys *.ocx *.cpl *.drv *.scr *.mui *.ax *.efi);;QDepends 会话 (*.qds);;所有文件 (*)"
    "&Save Session As..." = "会话另存为(&S)..."
    "Save Report As &Text..." = "报告另存为文本(&T)..."
    "Export Module List As &CSV..." = "导出模块列表为 CSV(&C)..."
    "&Recent Files" = "最近打开(&R)"
    "E&xit" = "退出(&X)"
    "&Edit" = "编辑(&E)"
    "&Copy" = "复制(&C)"
    "&Find..." = "查找(&F)..."
    "Find &Next" = "查找下一个(&N)"
    "Find &Previous" = "查找上一个(&P)"
    "Clear &Log" = "清空日志(&L)"
    "&View" = "视图(&V)"
    "Full &Paths" = "完整路径(&P)"
    "&Undecorate C++ Functions" = "反修饰 C++ 函数名(&U)"
    "&Expand All" = "全部展开(&E)"
    "Co&llapse All" = "全部折叠(&L)"
    "&Refresh" = "刷新(&R)"
    "Show Parent &Imports Pane" = "显示父导入窗格(&I)"
    "Show Export&s Pane" = "显示导出窗格(&S)"
    "Show &Module List Pane" = "显示模块列表窗格(&M)"
    "Show Lo&g Pane" = "显示日志窗格(&G)"
    "System &Information..." = "系统信息(&I)..."
    "&Options" = "选项(&O)"
    "Configure &External Viewer..." = "配置外部查看器(&E)..."
    "&Help" = "帮助(&H)"
    "&About QDepends" = "关于 QDepends(&A)"
    "About QDepends" = "关于 QDepends"
    "<b>QDepends 1.0</b><br/>A static PE dependency analyzer, functionally aligned with Dependency Walker.<br/><br/>Supports 32-bit (PE32) and 64-bit (PE32+) modules, API set redirection, KnownDLLs, side-by-side manifests and forwarded exports.<br/><br/>Built with Qt %1." = "<b>QDepends 1.0</b><br/>静态 PE 依赖分析工具，功能对齐 Dependency Walker。<br/><br/>支持 32 位 (PE32) 与 64 位 (PE32+) 模块、API Set 重定向、KnownDLLs、SxS 清单及转发导出。<br/><br/>基于 Qt %1 构建。"
    "Main" = "主工具栏"
    "Ready" = "就绪"
    "Analyzing %1..." = "正在分析 %1..."
    "File not found: %1" = "未找到文件: %1"
    "QDepends" = "QDepends"
    "Cannot load session: %1" = "无法加载会话: %1"
    "Modules: %1   Errors: %2   Warnings: %3" = "模块: %1   错误: %2   警告: %3"
    "Module not found" = "未找到模块"
    "Delay-load dependency" = "延迟加载依赖"
    "Forwarded dependency" = "转发依赖"
    "Duplicate module (already expanded above)" = "重复模块（已在上方展开）"
    "Dependency Tree" = "依赖树"
    "Parent Imports" = "父导入"
    "Exports" = "导出"
    "Module List" = "模块列表"
    "Log" = "日志"
    "Parent Imports — %1 ← %2" = "父导入 — %1 ← %2"
    "Parent Imports — (root module)" = "父导入 — （根模块）"
    "Exports — %1" = "导出 — %1"
    "Expand Subtree" = "展开子树"
    "Copy File Path" = "复制文件路径"
    "Show in Explorer" = "在资源管理器中显示"
    "View Module in External Viewer" = "用外部查看器打开模块"
    "Locate in Module List" = "在模块列表中定位"
    "Save Session" = "保存会话"
    "QDepends Session (*.qds)" = "QDepends 会话 (*.qds)"
    "Cannot save session: %1" = "无法保存会话: %1"
    "Save Report" = "保存报告"
    "Text Files (*.txt)" = "文本文件 (*.txt)"
    "Export CSV" = "导出 CSV"
    "CSV Files (*.csv)" = "CSV 文件 (*.csv)"
    "Report saved to %1" = "报告已保存至 %1"
    "CSV exported to %1" = "CSV 已导出至 %1"
    "External Viewer" = "外部查看器"
    "Command line (%1 is replaced with the module path):" = "命令行（%1 将被替换为模块路径）:"
    "System Information" = "系统信息"
    "Close" = "关闭"
}

[xml]$xml = Get-Content $tsPath -Raw -Encoding UTF8
$filled = 0
$missing = @()
foreach ($context in $xml.SelectNodes("/TS/context")) {
    foreach ($msg in $context.SelectNodes("message")) {
        $src = $msg.SelectSingleNode("source").InnerText
        $tr = $msg.SelectSingleNode("translation")
        if ($dict.ContainsKey($src)) {
            $tr.RemoveAttribute("type")
            $tr.InnerText = $dict[$src]
            $filled++
        } else {
            $missing += "$($context.SelectSingleNode('name').InnerText): $src"
        }
    }
}

$settings = New-Object System.Xml.XmlWriterSettings
$settings.Indent = $true
$settings.Encoding = New-Object System.Text.UTF8Encoding($false)
$writer = [System.Xml.XmlWriter]::Create($tsPath, $settings)
$xml.Save($writer)
$writer.Close()

"filled=$filled"
if ($missing.Count -gt 0) { "MISSING:"; $missing }
