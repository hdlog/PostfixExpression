
#include <graphics.h>   // EasyX 图形库
#include <windows.h>
#include "ppe.h"  // 引入表达式树核心逻辑

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>

// ===================== 全局字号常量 =====================
const int FONT_TITLE  = 26;  // 标题
const int FONT_NORMAL = 20;  // 正文
const int FONT_SMALL  = 18;  // 状态/提示

// ===================== UI 工具函数 =====================
// 字符串转宽字符串（用于 EasyX 输出中文等）
static std::wstring s2ws(const std::string& s) {
    if (s.empty()) return L"";
    // 使用 CP_ACP（本地代码页，中文系统为 GBK）而非 CP_UTF8
    int len = MultiByteToWideChar(CP_ACP, 0, s.c_str(), -1, nullptr, 0);
    if (len <= 0) return L"";
    std::wstring ws(len, L'\0');
    MultiByteToWideChar(CP_ACP, 0, s.c_str(), -1, &ws[0], len);
    if (!ws.empty() && ws.back() == L'\0') ws.pop_back();
    return ws;
}

// 将 double 转为字符串，自动去除多余小数位
static string fmtDouble(double v) {
    char buf[64];
    if (std::fabs(v) < 1e-12) v = 0;
    std::snprintf(buf, sizeof(buf), "%.12g", v);
    return buf;
}

// 矩形结构体
struct RectI { int x, y, w, h; };

// 判断点 (mx, my) 是否在矩形 r 内
static bool hitRect(const RectI& r, int mx, int my) {
    return mx >= r.x && mx <= r.x + r.w && my >= r.y && my <= r.y + r.h;
}

// 绘制面板背景
static void drawPanelBg(int x, int y, int w, int h) {
    setfillcolor(RGB(248, 248, 248));
	solidrectangle(x, y, x + w, y + h); // 背景
	setlinecolor(RGB(210, 210, 210));   // 边框
    rectangle(x, y, x + w, y + h);      
}

// ===================== UI 组件 =====================

// 按钮结构体
struct Button {
    RectI rc{};
    string text;
    bool hot = false, down = false;

    bool hit(int x, int y) const { return hitRect(rc, x, y); }

    void draw() const {
        COLORREF bg = hot ? RGB(230, 240, 255) : RGB(245, 245, 245);
        if (down) bg = RGB(210, 225, 255);
        setfillcolor(bg);
		solidroundrect(rc.x, rc.y, rc.x + rc.w, rc.y + rc.h, 8, 8); // 圆角按钮
        setlinecolor(RGB(180, 180, 180));
        roundrect(rc.x, rc.y, rc.x + rc.w, rc.y + rc.h, 8, 8);  

        // 使用统一字号
        settextstyle(FONT_NORMAL, 0, L"Microsoft YaHei");
        setbkmode(TRANSPARENT);
        settextcolor(RGB(30, 30, 30));
        std::wstring ws = s2ws(text);
        int ty = rc.y + (rc.h - textheight(ws.c_str())) / 2;
        outtextxy(rc.x + 10, ty, ws.c_str());
    }
};
// 输入框结构体（支持选择、复制、粘贴）
struct TextBox {
    RectI rc{};
    string text;
    bool active = false;
    int cursorPos = 0;
    int selStart = -1;    // 选择起始位置，-1 表示无选择
    int selEnd = -1;      // 选择结束位置
    bool selecting = false; // 是否正在拖选

    bool hit(int x, int y) const { return hitRect(rc, x, y); }

    // 获取选择范围（确保 start <= end）
    void getSelection(int& start, int& end) const {
        if (selStart < 0 || selEnd < 0) {
            start = end = -1;
            return;
        }
        start = (std::min)(selStart, selEnd);
        end = (std::max)(selStart, selEnd);
    }

    // 判断是否有选择
    bool hasSelection() const {
        return selStart >= 0 && selEnd >= 0 && selStart != selEnd;
    }

    // 清除选择
    void clearSelection() {
        selStart = selEnd = -1;
        selecting = false;
    }

    // 删除选中文本
    void deleteSelection() {
        if (!hasSelection()) return;
        int start, end;
        getSelection(start, end);
		text.erase(start, end - start); // 删除选中部分
        cursorPos = start;
        clearSelection();
    }

    // 获取选中的文本
    string getSelectedText() const {
        if (!hasSelection()) return "";
        int start, end;
        getSelection(start, end);
        return text.substr(start, end - start);
    }

    // 新增：鼠标单击，定位光标并清除选择
    void onMouseClick(int mx, int my) {
        settextstyle(FONT_NORMAL, 0, L"Microsoft YaHei");
        int pos = posFromX(mx);
        cursorPos = pos;
        selStart = selEnd = -1;
        selecting = false;
    }
    // 复制到剪贴板
    void copyToClipboard() const {
        string sel = getSelectedText();
        if (sel.empty()) return;

        if (!OpenClipboard(GetHWnd())) return;
        EmptyClipboard();

        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, sel.size() + 1);
        if (hMem) {
            char* pMem = (char*)GlobalLock(hMem);
            if (pMem) {
                memcpy(pMem, sel.c_str(), sel.size() + 1);
                GlobalUnlock(hMem);
                SetClipboardData(CF_TEXT, hMem);
            }
        }
        CloseClipboard();
    }

    // 从剪贴板粘贴
    void pasteFromClipboard() {
        if (!OpenClipboard(GetHWnd())) return;

        HANDLE hData = GetClipboardData(CF_TEXT);
        if (hData) {
            char* pData = (char*)GlobalLock(hData);
            if (pData) {
                string pasteText = pData;
                GlobalUnlock(hData);

                // 如果有选择，先删除
                if (hasSelection()) {
                    deleteSelection();
                }

                // 过滤非法字符，只保留可打印字符
                string filtered;
                for (char c : pasteText) {
                    if (c >= 32 && c <= 126) {
                        filtered += c;
                    }
                }

                // 插入文本
                text.insert(cursorPos, filtered);
                cursorPos += (int)filtered.size();
            }
        }
        CloseClipboard();
    }

    // 全选
    void selectAll() {
        if (text.empty()) return;
        selStart = 0;
        selEnd = (int)text.size();
        cursorPos = selEnd;
    }

    void draw(const string& hint = "") const {
        setfillcolor(active ? RGB(255, 255, 255) : RGB(250, 250, 250));
        solidrectangle(rc.x, rc.y, rc.x + rc.w, rc.y + rc.h);
        setlinecolor(active ? RGB(80, 140, 255) : RGB(180, 180, 180));
        rectangle(rc.x, rc.y, rc.x + rc.w, rc.y + rc.h);

        settextstyle(FONT_NORMAL, 0, L"Microsoft YaHei");
        setbkmode(TRANSPARENT);

        int ty = rc.y + (rc.h - textheight(L"A")) / 2;

        // 绘制选择高亮
        if (active && hasSelection()) {
            int start, end;
            getSelection(start, end);

            string beforeStart = text.substr(0, start);
            string selected = text.substr(start, end - start);

            int x1 = rc.x + 8 + textwidth(s2ws(beforeStart).c_str());
            int x2 = x1 + textwidth(s2ws(selected).c_str());

            // 绘制选择背景
            setfillcolor(RGB(51, 153, 255));
            solidrectangle(x1, rc.y + 4, x2, rc.y + rc.h - 4);
        }

        // 绘制文本
        string show = text.empty() ? hint : text;
        if (text.empty()) {
            settextcolor(RGB(140, 140, 140));
            outtextxy(rc.x + 8, ty, s2ws(show).c_str());
        }
        else {
            // 分段绘制（选中部分用白色）
            if (active && hasSelection()) {
                int start, end;
                getSelection(start, end);

                string before = text.substr(0, start);
                string sel = text.substr(start, end - start);
                string after = text.substr(end);

                int x = rc.x + 8;

                // 选择前
                settextcolor(RGB(20, 20, 20));
                outtextxy(x, ty, s2ws(before).c_str());
                x += textwidth(s2ws(before).c_str());

                // 选中部分（白色文字）
                settextcolor(RGB(255, 255, 255));
                outtextxy(x, ty, s2ws(sel).c_str());
                x += textwidth(s2ws(sel).c_str());

                // 选择后
                settextcolor(RGB(20, 20, 20));
                outtextxy(x, ty, s2ws(after).c_str());
            }
            else {
                settextcolor(RGB(20, 20, 20));
                outtextxy(rc.x + 8, ty, s2ws(text).c_str());
            }
        }

        // 光标闪烁（无选择时显示）
        if (active && !hasSelection()) {
			bool showCursor = ((GetTickCount() / 500) % 2) == 0;    // 每 500ms 切换一次
            if (showCursor) {
                string beforeCursor = text.substr(0, cursorPos);
                int cx = rc.x + 8 + textwidth(s2ws(beforeCursor).c_str());
                setlinecolor(RGB(60, 60, 60));
                line(cx, rc.y + 6, cx, rc.y + rc.h - 6);
            }
        }
    }

    // 字符输入处理
    void onChar(int ch) {
        if (!active) return;

        if (ch == 8) {  // Backspace
            if (hasSelection()) {
                deleteSelection();
            }
            else if (cursorPos > 0 && !text.empty()) {
                text.erase(cursorPos - 1, 1);
                cursorPos--;
            }
            return;
        }
        if (ch == 13) return;  // Enter 不处理

        if (ch >= 32 && ch <= 126) {
            // 如果有选择，先删除选中内容
            if (hasSelection()) {
                deleteSelection();
            }
            text.insert(cursorPos, 1, (char)ch);
            cursorPos++;
        }
    }

    // 按键处理（支持 Ctrl+A/C/V/X，Shift+方向键选择）
    void onKeyDown(int vk, bool shift, bool ctrl) {
        if (!active) return;

        // Ctrl+A 全选
        if (ctrl && vk == 'A') {
            selectAll();
            return;
        }

        // Ctrl+C 复制
        if (ctrl && vk == 'C') {
            copyToClipboard();
            return;
        }

        // Ctrl+V 粘贴
        if (ctrl && vk == 'V') {
            pasteFromClipboard();
            return;
        }

        // Ctrl+X 剪切
        if (ctrl && vk == 'X') {
            copyToClipboard();
            deleteSelection();
            return;
        }

        // 方向键移动（Shift 选择）
        switch (vk) {
        case VK_LEFT:
            if (shift) {
                if (selStart < 0) selStart = cursorPos;
                if (cursorPos > 0) cursorPos--;
                selEnd = cursorPos;
            }
            else {
                if (hasSelection()) {
                    int start, end;
                    getSelection(start, end);
                    cursorPos = start;
                    clearSelection();
                }
                else if (cursorPos > 0) {
                    cursorPos--;
                }
            }
            break;

        case VK_RIGHT:
            if (shift) {
                if (selStart < 0) selStart = cursorPos;
                if (cursorPos < (int)text.size()) cursorPos++;
                selEnd = cursorPos;
            }
            else {
                if (hasSelection()) {
                    int start, end;
                    getSelection(start, end);
                    cursorPos = end;
                    clearSelection();
                }
                else if (cursorPos < (int)text.size()) {
                    cursorPos++;
                }
            }
            break;

        case VK_HOME:
            if (shift) {
                if (selStart < 0) selStart = cursorPos;
                cursorPos = 0;
                selEnd = cursorPos;
            }
            else {
                clearSelection();
                cursorPos = 0;
            }
            break;

        case VK_END:
            if (shift) {
                if (selStart < 0) selStart = cursorPos;
                cursorPos = (int)text.size();
                selEnd = cursorPos;
            }
            else {
                clearSelection();
                cursorPos = (int)text.size();
            }
            break;

        case VK_DELETE:
            if (hasSelection()) {
                deleteSelection();
            }
            else if (cursorPos < (int)text.size()) {
                text.erase(cursorPos, 1);
            }
            break;
        }
    }

    // 鼠标按下：开始选择或定位光标
    void onMouseDown(int mx, int my) {
        if (!hit(mx, my)) return;

        settextstyle(FONT_NORMAL, 0, L"Microsoft YaHei");

        int pos = posFromX(mx);
        cursorPos = pos;
        selStart = pos;
        selEnd = pos;
        selecting = true;
    }

    // 鼠标移动：拖选
    void onMouseMove(int mx, int my) {
        if (!selecting) return;

        settextstyle(FONT_NORMAL, 0, L"Microsoft YaHei");

        int pos = posFromX(mx);
        selEnd = pos;
        cursorPos = pos;
    }

    // 鼠标释放：结束选择
    void onMouseUp(int mx, int my) {
        selecting = false;
        // 如果起点终点相同，清除选择
        if (selStart == selEnd) {
            clearSelection();
        }
    }

    // 双击：选中整个单词或全选
    void onDoubleClick(int mx, int my) {
        if (!hit(mx, my)) return;
        selectAll();
    }

    // 根据 x 坐标计算字符位置
    int posFromX(int mx) const {
        int relX = mx - rc.x - 8;
        if (relX <= 0) return 0;

        int bestPos = 0;
        int bestDist = relX;

        for (int i = 0; i <= (int)text.size(); ++i) {
            string sub = text.substr(0, i);
            int w = textwidth(s2ws(sub).c_str());
            int dist = std::abs(relX - w);
            if (dist < bestDist) {
                bestDist = dist;
                bestPos = i;
            }
        }
        return bestPos;
    }
	// 激活输入框
    void activate() {
        active = true;
        if (cursorPos > (int)text.size()) {
            cursorPos = (int)text.size();
        }
    }

	// 清空内容
    void clear() {
        text.clear();
        cursorPos = 0;
        clearSelection();
    }
};
// ===================== 弹窗函数 =====================

// 弹窗输入一个数字（变量赋值用）
static bool ModalInputNumber(const string& title, const string& hint, double& outVal) {
    int W = 520, H = 260;
    int x0 = (getwidth() - W) / 2;
    int y0 = (getheight() - H) / 2;

    TextBox tb;
    tb.rc = { x0 + 20, y0 + 100, W - 40, 44 };
	tb.activate();  // 使用 activate 方法,确保光标位置正确

    Button ok, cancel;
    ok.text = "确定"; ok.rc = { x0 + W - 220, y0 + H - 60, 90, 40 };
    cancel.text = "取消"; cancel.rc = { x0 + W - 120, y0 + H - 60, 90, 40 };

    ExMessage msg{};
    while (true) {
        BeginBatchDraw();
        setfillcolor(RGB(235, 235, 235));
        solidrectangle(0, 0, getwidth(), getheight());

        setfillcolor(WHITE);
        setlinecolor(RGB(130, 130, 130));
        fillroundrect(x0, y0, x0 + W, y0 + H, 10, 10);

        setbkmode(TRANSPARENT);
        settextstyle(FONT_TITLE, 0, L"Microsoft YaHei");
        settextcolor(RGB(30, 30, 30));
        outtextxy(x0 + 20, y0 + 20, s2ws(title).c_str());

        settextstyle(FONT_NORMAL, 0, L"Microsoft YaHei");
        outtextxy(x0 + 20, y0 + 65, L"请输入数值：");
        tb.draw(hint);
        ok.draw(); cancel.draw();
        FlushBatchDraw();

        while (peekmessage(&msg, EM_MOUSE | EM_KEY | EM_CHAR)) {
            if (msg.message == WM_MOUSEMOVE) {
                ok.hot = ok.hit(msg.x, msg.y);
                cancel.hot = cancel.hit(msg.x, msg.y);
            }
            else if (msg.message == WM_LBUTTONDOWN) {
                ok.down = ok.hit(msg.x, msg.y);
                cancel.down = cancel.hit(msg.x, msg.y);
                // 点击输入框时定位光标
                if (tb.hit(msg.x, msg.y)) {
                    tb.activate();
                    tb.onMouseClick(msg.x, msg.y);
                }
                else {
                    tb.active = false;
                }
            }
            else if (msg.message == WM_LBUTTONUP) {
                bool okClick = ok.down && ok.hit(msg.x, msg.y);
                bool cancelClick = cancel.down && cancel.hit(msg.x, msg.y);
                ok.down = cancel.down = false;

                if (cancelClick) return false;
                if (okClick) {
                    // 空输入时提示并返回 false
                    if (tb.text.empty()) {
                        MessageBoxW(GetHWnd(), L"输入为空，请输入数值或点击取消。", L"提示", MB_OK);
                        continue;
                    }
                    char* endp = nullptr;
                    double v = strtod(tb.text.c_str(), &endp);
                    if (endp && *endp == '\0') { outVal = v; return true; }
                    MessageBoxW(GetHWnd(), L"输入不是合法数字。", L"提示", MB_OK);
                }
            }
            else if (msg.message == WM_CHAR) {
                tb.onChar((int)msg.ch);
            }
            else if (msg.message == WM_KEYDOWN) {
                // 处理方向键、Home、End、Delete

                tb.onKeyDown(msg.vkcode, msg.shift, msg.ctrl);

                if (msg.vkcode == VK_ESCAPE) return false;
                if (msg.vkcode == VK_RETURN) {
                    // 空输入时不处理
                    if (tb.text.empty()) {
                        MessageBoxW(GetHWnd(), L"输入为空，请输入数值或点击取消。", L"提示", MB_OK);
                        continue;
                    }
                    char* endp = nullptr;
                    double v = strtod(tb.text.c_str(), &endp);
                    if (endp && *endp == '\0') { outVal = v; return true; }
                }
            }
        }
        Sleep(10);
    }
}

// 弹窗选择槽位
static bool ModalPickSlot(int slotN, const wchar_t* title, int& outIdx) {
    int W = 560, H = 300;
    int x0 = (getwidth() - W) / 2, y0 = (getheight() - H) / 2;

    vector<Button> bs;
    int bw = 100, bh = 40, gap = 12;
    int startX = x0 + 30, startY = y0 + 90;
    for (int i = 0; i < slotN; i++) {
        Button b;
        b.text = "槽位" + std::to_string(i + 1);
        int r = i / 4, c = i % 4;
        b.rc = { startX + c * (bw + gap), startY + r * (bh + gap), bw, bh };
        bs.push_back(b);
    }

    Button cancel; cancel.text = "取消"; cancel.rc = { x0 + W - 120, y0 + H - 60, 100, 40 };

    ExMessage msg{};
    while (true) {
		BeginBatchDraw();   // 开始批量绘制
        setfillcolor(RGB(235, 235, 235));
        solidrectangle(0, 0, getwidth(), getheight());
        setfillcolor(WHITE);
        setlinecolor(RGB(130, 130, 130));
        fillroundrect(x0, y0, x0 + W, y0 + H, 10, 10);

		setbkmode(TRANSPARENT); // 透明背景
        settextstyle(FONT_TITLE, 0, L"Microsoft YaHei");
        settextcolor(RGB(30, 30, 30));
        outtextxy(x0 + 20, y0 + 20, title);
        
        settextstyle(FONT_NORMAL, 0, L"Microsoft YaHei");
        outtextxy(x0 + 20, y0 + 55, L"请选择目标槽位：");

        for (auto& b : bs) b.draw();
		cancel.draw();  // 绘制取消按钮
        FlushBatchDraw();
		// 结束批量绘制
        while (peekmessage(&msg, EM_MOUSE | EM_KEY)) {
            if (msg.message == WM_MOUSEMOVE) {
                for (auto& b : bs) b.hot = b.hit(msg.x, msg.y);
                cancel.hot = cancel.hit(msg.x, msg.y);
            }
            else if (msg.message == WM_LBUTTONDOWN) {
                for (auto& b : bs) b.down = b.hit(msg.x, msg.y);
                cancel.down = cancel.hit(msg.x, msg.y);
            }
            else if (msg.message == WM_LBUTTONUP) {
                auto click = [&](Button& b) { bool c = b.down && b.hit(msg.x, msg.y); b.down = false; return c; };
                if (click(cancel)) return false;
                for (int i = 0; i < (int)bs.size(); i++) {
                    if (click(bs[i])) { outIdx = i; return true; }
                }
            }
            else if (msg.message == WM_KEYDOWN) {
                if (msg.vkcode == VK_ESCAPE) return false;
            }
        }
        Sleep(10);
    }
}

// 弹窗选择运算符
static bool ModalPickOperator(char& op) {
    int W = 500, H = 220;
    int x0 = (getwidth() - W) / 2;
    int y0 = (getheight() - H) / 2;

    vector<Button> ops;
    int ox = x0 + 40, oy = y0 + 90;
    int bw = 70, bh = 40, gap = 12;
    const char* opList = "+-*/^";
    for (int i = 0; i < 5; i++) {
        Button b; b.text = string(1, opList[i]);
        b.rc = { ox + i * (bw + gap), oy, bw, bh };
        ops.push_back(b);
    }
	// 取消按钮
    Button cancel; cancel.text = "取消"; cancel.rc = { x0 + W - 120, y0 + H - 60, 100, 40 };
    
    ExMessage msg{};
    
    while (true) {
        BeginBatchDraw();
        setfillcolor(RGB(235, 235, 235));
        solidrectangle(0, 0, getwidth(), getheight());

        setfillcolor(WHITE);
        setlinecolor(RGB(130, 130, 130));
        fillroundrect(x0, y0, x0 + W, y0 + H, 10, 10);

        setbkmode(TRANSPARENT);
        settextstyle(FONT_TITLE, 0, L"Microsoft YaHei");
        settextcolor(RGB(30, 30, 30));
        outtextxy(x0 + 20, y0 + 20, L"选择运算符 P");
        
        settextstyle(FONT_NORMAL, 0, L"Microsoft YaHei");
        outtextxy(x0 + 20, y0 + 55, L"点击选择：");

        for (auto& b : ops) b.draw();
        cancel.draw();
        FlushBatchDraw();

        while (peekmessage(&msg, EM_MOUSE | EM_KEY)) {
            if (msg.message == WM_MOUSEMOVE) {
                for (auto& b : ops) b.hot = b.hit(msg.x, msg.y);
                cancel.hot = cancel.hit(msg.x, msg.y);
            }
            else if (msg.message == WM_LBUTTONDOWN) {
                for (auto& b : ops) b.down = b.hit(msg.x, msg.y);
                cancel.down = cancel.hit(msg.x, msg.y);
            }
            else if (msg.message == WM_LBUTTONUP) {
                auto click = [&](Button& b) { bool c = b.down && b.hit(msg.x, msg.y); b.down = false; return c; };
                if (click(cancel)) return false;
                for (int i = 0; i < (int)ops.size(); i++) {
                    if (click(ops[i])) { op = opList[i]; return true; }
                }
            }
            else if (msg.message == WM_KEYDOWN) {
                if (msg.vkcode == VK_ESCAPE) return false;
            }
        }
        Sleep(10);
    }
}

// 弹窗选择变量（求偏导用）
static bool ModalPickVar(const vector<char>& vars, char& outVar) {
    if (vars.empty()) return false;

    int W = 560, H = 300;
    int x0 = (getwidth() - W) / 2;
    int y0 = (getheight() - H) / 2;

    vector<Button> bs;
    int cols = 6;
    int bw = 70, bh = 40, gap = 10;
    int startX = x0 + 30, startY = y0 + 90;

    for (int i = 0; i < (int)vars.size(); ++i) {
        Button b;
        b.text = string(1, vars[i]);
        int r = i / cols, c = i % cols;
        b.rc = { startX + c * (bw + gap), startY + r * (bh + gap), bw, bh };
        bs.push_back(b);
    }

    Button ok, cancel;
    ok.text = "确定"; ok.rc = { x0 + W - 220, y0 + H - 60, 100, 40 };
    cancel.text = "取消"; cancel.rc = { x0 + W - 110, y0 + H - 60, 100, 40 };

    int sel = 0;
    ExMessage msg{};
    while (true) {
        BeginBatchDraw();
        setfillcolor(RGB(235, 235, 235));
        solidrectangle(0, 0, getwidth(), getheight());

        setfillcolor(WHITE);
        setlinecolor(RGB(130, 130, 130));
        fillroundrect(x0, y0, x0 + W, y0 + H, 10, 10);

        setbkmode(TRANSPARENT);
        settextstyle(FONT_TITLE, 0, L"Microsoft YaHei");
        settextcolor(RGB(30, 30, 30));
        outtextxy(x0 + 20, y0 + 20, L"选择对哪个变量求偏导");
        
        settextstyle(FONT_NORMAL, 0, L"Microsoft YaHei");
        outtextxy(x0 + 20, y0 + 55, L"（表达式内出现的未知数如下）");

        for (int i = 0; i < (int)bs.size(); ++i) {
            bool isSel = (i == sel);
            COLORREF bg = isSel ? RGB(210, 225, 255) : (bs[i].hot ? RGB(230, 240, 255) : RGB(245, 245, 245));
            setfillcolor(bg);
            solidroundrect(bs[i].rc.x, bs[i].rc.y, bs[i].rc.x + bs[i].rc.w, bs[i].rc.y + bs[i].rc.h, 8, 8);
            setlinecolor(RGB(180, 180, 180));
            roundrect(bs[i].rc.x, bs[i].rc.y, bs[i].rc.x + bs[i].rc.w, bs[i].rc.y + bs[i].rc.h, 8, 8);
            
            settextstyle(FONT_NORMAL, 0, L"Microsoft YaHei");
            int tx = bs[i].rc.x + (bs[i].rc.w - textwidth(s2ws(bs[i].text).c_str())) / 2;
            int ty = bs[i].rc.y + (bs[i].rc.h - textheight(L"A")) / 2;
            outtextxy(tx, ty, s2ws(bs[i].text).c_str());
        }

        ok.draw(); cancel.draw();
        FlushBatchDraw();

        while (peekmessage(&msg, EM_MOUSE | EM_KEY)) {
            if (msg.message == WM_MOUSEMOVE) {
                for (auto& b : bs) b.hot = b.hit(msg.x, msg.y);
                ok.hot = ok.hit(msg.x, msg.y);
                cancel.hot = cancel.hit(msg.x, msg.y);
            }
            else if (msg.message == WM_LBUTTONDOWN) {
                for (auto& b : bs) b.down = b.hit(msg.x, msg.y);
                ok.down = ok.hit(msg.x, msg.y);
                cancel.down = cancel.hit(msg.x, msg.y);
            }
            else if (msg.message == WM_LBUTTONUP) {
                auto click = [&](Button& b) { bool c = b.down && b.hit(msg.x, msg.y); b.down = false; return c; };
                if (click(cancel)) return false;
                for (int i = 0; i < (int)bs.size(); ++i) {
                    if (click(bs[i])) sel = i;
                }
                if (click(ok)) {
                    outVar = vars[sel];
                    return true;
                }
            }
            
            else if (msg.message == WM_KEYDOWN) {
                if (msg.vkcode == VK_ESCAPE) return false;
                if (msg.vkcode == VK_RETURN) { outVar = vars[sel]; return true; }
                if (msg.vkcode == VK_LEFT && sel > 0) sel--;
                if (msg.vkcode == VK_RIGHT && sel + 1 < (int)vars.size()) sel++;
            }
        }
        Sleep(10);
    }
}

// ===================== 一步弹窗：选择 E1/E2/P =====================
static bool ModalComposeBest(
	const std::vector<bool>& hasSlot,   // 槽位是否有表达式
	const std::vector<ExprTree>& slots, // 槽位表达式
	int& outE1, int& outE2, char& outOp // 输出选择结果
) {
    const int n = (int)hasSlot.size();
    
    // ★增大弹窗尺寸，调整布局
    int W = 960, H = 620;
    int x0 = (getwidth() - W) / 2;
    int y0 = (getheight() - H) / 2;

    // ★调整各面板位置和尺寸，增加间距
    RectI panelSlots = { x0 + 20, y0 + 100, 565, 420 };  // 左侧槽位面板
    RectI panelOps = { x0 + 600, y0 + 100, 340, 140 };   // 右上运算符面板
    RectI panelPrev = { x0 + 600, y0 + 260, 340, 260 };  // 右下预览面板

    // 槽位网格 - 调整按钮尺寸和间距
    int cols = 4;
    int bw = 128, bh = 52, gap = 12;
    int startX = panelSlots.x + 12;
    int startY = panelSlots.y + 12;

    std::vector<Button> slotBtns;
    slotBtns.reserve(n);
    for (int i = 0; i < n; ++i) {
        Button b;
        b.text = "槽位" + std::to_string(i + 1);
        int r = i / cols, c = i % cols;
        b.rc = { startX + c * (bw + gap), startY + r * (bh + gap), bw, bh };
        slotBtns.push_back(b);
    }

    // 运算符按钮 - 调整尺寸和间距
    std::vector<Button> opBtns;
    const char* ops = "+-*/^";
    int obw = 56, obh = 44;
    int ox = panelOps.x + 15, oy = panelOps.y + 60;
    for (int i = 0; i < 5; ++i) {
        Button b;
        b.text = string(1, ops[i]);
        b.rc = { ox + i * (obw + 10), oy, obw, obh };
        opBtns.push_back(b);
    }

    Button ok, cancel;
    ok.text = "确定";
    ok.rc = { x0 + W - 230, y0 + H - 60, 100, 44 };
    cancel.text = "取消";
    cancel.rc = { x0 + W - 115, y0 + H - 60, 100, 44 };

    int selE1 = 0, selE2 = 1;
    if (selE2 >= n) selE2 = 0;
    char selOp = '+';
    int pickMode = 1;

    auto slotPreview = [&](int idx)->string {
        if (idx < 0 || idx >= n) return "<无>";
        if (!hasSlot[idx] || !slots[idx].root) return "<空>";
        string s = slots[idx].toInfix();
        if ((int)s.size() > 40) s = s.substr(0, 40) + "...";  // 缩短预览长度
        return s;
    };

    ExMessage msg{};
    while (true) {
        BeginBatchDraw();
        setfillcolor(RGB(235, 235, 235));
        solidrectangle(0, 0, getwidth(), getheight());

        setfillcolor(WHITE);
        setlinecolor(RGB(130, 130, 130));
        fillroundrect(x0, y0, x0 + W, y0 + H, 12, 12);

        setbkmode(TRANSPARENT);
        settextstyle(FONT_TITLE, 0, L"Microsoft YaHei");
        settextcolor(RGB(30, 30, 30));
        outtextxy(x0 + 24, y0 + 20, L"一步构造复合表达式： (E1) P (E2)");
        
        settextstyle(FONT_SMALL, 0, L"Microsoft YaHei");
        outtextxy(x0 + 24, y0 + 55, L"操作：先选择\"正在选E1/正在选E2\"，再点槽位；右侧选运算符 P。");

        // 选择模式按钮 - 调整位置
        RectI modeE1 = { x0 + 420, y0 + 50, 140, 34 };
        RectI modeE2 = { x0 + 570, y0 + 50, 140, 34 };
        auto drawMode = [&](RectI r, const wchar_t* t, bool on) {
            setfillcolor(on ? RGB(210, 225, 255) : RGB(245, 245, 245));
            solidroundrect(r.x, r.y, r.x + r.w, r.y + r.h, 8, 8);
            setlinecolor(on ? RGB(80, 140, 255) : RGB(180, 180, 180));
            roundrect(r.x, r.y, r.x + r.w, r.y + r.h, 8, 8);
            settextstyle(FONT_SMALL, 0, L"Microsoft YaHei");
            settextcolor(RGB(30, 30, 30));
            int tx = r.x + (r.w - textwidth(t)) / 2;
            int ty = r.y + (r.h - textheight(t)) / 2;
            outtextxy(tx, ty, t);
        };
        drawMode(modeE1, L"正在选 E1", pickMode == 1);
        drawMode(modeE2, L"正在选 E2", pickMode == 2);

        // 槽位面板
        setfillcolor(RGB(250, 250, 250));
        solidroundrect(panelSlots.x, panelSlots.y, panelSlots.x + panelSlots.w, panelSlots.y + panelSlots.h, 8, 8);
        setlinecolor(RGB(200, 200, 200));
        roundrect(panelSlots.x, panelSlots.y, panelSlots.x + panelSlots.w, panelSlots.y + panelSlots.h, 8, 8);
        settextstyle(FONT_SMALL, 0, L"Microsoft YaHei");
        settextcolor(RGB(80, 80, 80));
        outtextxy(panelSlots.x + 12, panelSlots.y - 24, L"选择槽位（空槽位显示灰色）：");

        settextstyle(FONT_NORMAL, 0, L"Microsoft YaHei");
        for (int i = 0; i < n; ++i) {
            auto& b = slotBtns[i];
            bool isEmpty = !hasSlot[i] || !slots[i].root;
            bool isSelE1 = (i == selE1);
            bool isSelE2 = (i == selE2);

            COLORREF bg = RGB(248, 248, 248);
            if (b.hot) bg = RGB(230, 240, 255);
            if (b.down) bg = RGB(210, 225, 255);
            if (isEmpty) bg = RGB(235, 235, 235);

            setfillcolor(bg);
            solidroundrect(b.rc.x, b.rc.y, b.rc.x + b.rc.w, b.rc.y + b.rc.h, 8, 8);

            // 选中状态用粗边框
            if (isSelE1) {
                setlinecolor(RGB(70, 130, 255));
                setlinestyle(PS_SOLID, 2);
            } else if (isSelE2) {
                setlinecolor(RGB(255, 140, 0));
                setlinestyle(PS_SOLID, 2);
            } else {
                setlinecolor(RGB(180, 180, 180));
                setlinestyle(PS_SOLID, 1);
            }
            roundrect(b.rc.x, b.rc.y, b.rc.x + b.rc.w, b.rc.y + b.rc.h, 8, 8);
            setlinestyle(PS_SOLID, 1);  // 恢复默认

            settextcolor(isEmpty ? RGB(140, 140, 140) : RGB(30, 30, 30));
            outtextxy(b.rc.x + 12, b.rc.y + 16, s2ws(b.text).c_str());

            // E1/E2 标签放在按钮右侧
            if (isSelE1) {
                settextcolor(RGB(70, 130, 255));
                outtextxy(b.rc.x + b.rc.w - 30, b.rc.y + 16, L"E1");
            }
            if (isSelE2) {
                settextcolor(RGB(255, 140, 0));
                outtextxy(b.rc.x + b.rc.w - 30, b.rc.y + 16, L"E2");
            }
        }

        // 运算符面板
        setfillcolor(RGB(250, 250, 250));
        solidroundrect(panelOps.x, panelOps.y, panelOps.x + panelOps.w, panelOps.y + panelOps.h, 8, 8);
        setlinecolor(RGB(200, 200, 200));
        roundrect(panelOps.x, panelOps.y, panelOps.x + panelOps.w, panelOps.y + panelOps.h, 8, 8);
        settextstyle(FONT_SMALL, 0, L"Microsoft YaHei");
        settextcolor(RGB(80, 80, 80));
        outtextxy(panelOps.x + 15, panelOps.y + 18, L"选择运算符 P：");

        settextstyle(FONT_NORMAL, 0, L"Microsoft YaHei");
        for (auto& b : opBtns) {
            bool sel = (b.text[0] == selOp);
            COLORREF bg = sel ? RGB(210, 225, 255) : (b.hot ? RGB(230, 240, 255) : RGB(248, 248, 248));
            setfillcolor(bg);
            solidroundrect(b.rc.x, b.rc.y, b.rc.x + b.rc.w, b.rc.y + b.rc.h, 8, 8);
            setlinecolor(sel ? RGB(80, 140, 255) : RGB(180, 180, 180));
            roundrect(b.rc.x, b.rc.y, b.rc.x + b.rc.w, b.rc.y + b.rc.h, 8, 8);
            settextcolor(RGB(30, 30, 30));
            int tx = b.rc.x + (b.rc.w - textwidth(s2ws(b.text).c_str())) / 2;
            int ty = b.rc.y + (b.rc.h - textheight(L"A")) / 2;
            outtextxy(tx, ty, s2ws(b.text).c_str());
        }

        // 预览面板
        setfillcolor(RGB(250, 250, 250));
        solidroundrect(panelPrev.x, panelPrev.y, panelPrev.x + panelPrev.w, panelPrev.y + panelPrev.h, 8, 8);
        setlinecolor(RGB(200, 200, 200));
        roundrect(panelPrev.x, panelPrev.y, panelPrev.x + panelPrev.w, panelPrev.y + panelPrev.h, 8, 8);
        
        settextstyle(FONT_SMALL, 0, L"Microsoft YaHei");
        settextcolor(RGB(80, 80, 80));
        outtextxy(panelPrev.x + 15, panelPrev.y + 18, L"预览（中缀表达式）：");
        
        int prevLineH = 50;
        int prevY = panelPrev.y + 55;
        
        // E1 预览
        settextcolor(RGB(70, 130, 255));
        outtextxy(panelPrev.x + 15, prevY, L"E1:");
        settextcolor(RGB(50, 50, 50));
        outtextxy(panelPrev.x + 50, prevY, s2ws(slotPreview(selE1)).c_str());
        
        // E2 预览
        prevY += prevLineH;
        settextcolor(RGB(255, 140, 0));
        outtextxy(panelPrev.x + 15, prevY, L"E2:");
        settextcolor(RGB(50, 50, 50));
        outtextxy(panelPrev.x + 50, prevY, s2ws(slotPreview(selE2)).c_str());
        
        // 运算符预览
        prevY += prevLineH;
        settextcolor(RGB(80, 80, 80));
        outtextxy(panelPrev.x + 15, prevY, L"P :");
        settextcolor(RGB(50, 50, 50));
        outtextxy(panelPrev.x + 50, prevY, s2ws(string(1, selOp)).c_str());
        
        // 结果预览
        prevY += prevLineH;
        settextcolor(RGB(0, 120, 60));
        outtextxy(panelPrev.x + 15, prevY, L"结果：");
        string resultPreview = "(" + slotPreview(selE1) + ") " + selOp + " (" + slotPreview(selE2) + ")";
        if ((int)resultPreview.size() > 35) resultPreview = resultPreview.substr(0, 35) + "...";
        outtextxy(panelPrev.x + 15, prevY + 25, s2ws(resultPreview).c_str());

        ok.draw(); cancel.draw();
        FlushBatchDraw();

        while (peekmessage(&msg, EM_MOUSE | EM_KEY)) {
            if (msg.message == WM_MOUSEMOVE) {
                for (auto& b : slotBtns) b.hot = b.hit(msg.x, msg.y);
                for (auto& b : opBtns) b.hot = b.hit(msg.x, msg.y);
                ok.hot = ok.hit(msg.x, msg.y);
                cancel.hot = cancel.hit(msg.x, msg.y);
            }
            else if (msg.message == WM_LBUTTONDOWN) {
                for (auto& b : slotBtns) b.down = b.hit(msg.x, msg.y);
                for (auto& b : opBtns) b.down = b.hit(msg.x, msg.y);
                ok.down = ok.hit(msg.x, msg.y);
                cancel.down = cancel.hit(msg.x, msg.y);
            }
            else if (msg.message == WM_LBUTTONUP) {
                auto click = [&](Button& b) { bool c = b.down && b.hit(msg.x, msg.y); b.down = false; return c; };

                if (click(cancel)) return false;

                if (hitRect(modeE1, msg.x, msg.y)) pickMode = 1;
                if (hitRect(modeE2, msg.x, msg.y)) pickMode = 2;

                for (int i = 0; i < n; ++i) {
                    if (click(slotBtns[i])) {
                        if (pickMode == 1) selE1 = i;
                        else selE2 = i;
                    }
                }

                for (auto& b : opBtns) {
                    if (click(b)) selOp = b.text[0];
                }
				// 确认按钮
                if (click(ok)) {
                    if (!hasSlot[selE1] || !slots[selE1].root) {
                        MessageBoxW(GetHWnd(), L"E1 槽位为空，请重新选择。", L"提示", MB_OK);
                        continue;
                    }
                    if (!hasSlot[selE2] || !slots[selE2].root) {
                        MessageBoxW(GetHWnd(), L"E2 槽位为空，请重新选择。", L"提示", MB_OK);
                        continue;
                    }
                    outE1 = selE1;
                    outE2 = selE2;
                    outOp = selOp;
                    return true;
                }
            }
            else if (msg.message == WM_KEYDOWN) {
                if (msg.vkcode == VK_ESCAPE) return false;
                if (msg.vkcode == VK_RETURN) {
                    if (!hasSlot[selE1] || !slots[selE1].root) {
                        MessageBoxW(GetHWnd(), L"E1 槽位为空，请重新选择。", L"提示", MB_OK);
                        continue;
                    }
                    if (!hasSlot[selE2] || !slots[selE2].root) {
                        MessageBoxW(GetHWnd(), L"E2 槽位为空，请重新选择。", L"提示", MB_OK);
                        continue;
                    }
                    outE1 = selE1;
                    outE2 = selE2;
                    outOp = selOp;
                    return true;
                }
                if (msg.vkcode == '1') pickMode = 1;
                if (msg.vkcode == '2') pickMode = 2;
            }
        }
        Sleep(10);
    }
}
// ===================== APP 状态 =====================

static const int SLOT_N = 8;    // 槽位数量

struct AppState;
static void refreshVars(AppState& A);
static void rebuildLayout(AppState& A);

struct AppState {
    int W = 1200, H = 720;
    int leftW = 420;
    
	TextBox tbInput;    // 输入框

	vector<Button> funcBtns;    // 函数按钮列表
    int scrollY = 0;
	int scrollMin = -99999; // 最小滚动值
    int scrollMax = 0;

    RectI funcView{};
    RectI varPanel{};

    string status;
    double lastValue = 0;

	ExprTree cur;   // 当前表达式树
	vector<ExprTree> slots; // 槽位表达式
	vector<bool> hasSlot;   // 槽位是否有表达式
    bool hasCur = false;
    int rightScrollY = 0;
    int rightScrollMin = 0;

    std::set<char> varsInCur;
	std::map<char, double> varVals; // 变量赋值

    int selectedVarIdx = -1;
    vector<char> varList;

    Layout lay{};

    Node* selectedNode = nullptr;

    std::vector<Node*> undoRoots;
    int undoMax = 20;
    
    struct UndoSnapshot {
        Node* root = nullptr;
        std::map<char, double> varVals;
    };
    std::vector<UndoSnapshot> undoSnapshots;
    //树视图切换
    int viewTreeIdx = -1;   // -1=显示当前表达式树；0..N-1=显示槽位树
    Layout viewLay{};       // 右侧当前显示树的布局

    //树缩放
    double treeZoom = 1.0;  // 缩放比例，1.0 = 100%
    static constexpr double ZOOM_MIN = 0.3;
    static constexpr double ZOOM_MAX = 3.0;

    // 树拖动
    bool treeDragging = false;   // 是否正在拖动
    int treeDragStartX = 0;      // 拖动起始鼠标位置
    int treeDragStartY = 0;
    int treeOffsetX = 0;         // 树的偏移量
    int treeOffsetY = 0;
};

// ===================== 撤销功能 =====================
// 撤销快照结构：保存树和变量赋值状态
struct UndoSnapshot {
    Node* root = nullptr;
    std::map<char, double> varVals;
};

//状态保存到撤销栈
static void PushUndo(AppState& A) {
    if (!A.hasCur || !A.cur.root) return;
    AppState::UndoSnapshot snap;
    snap.root = cloneTree(A.cur.root);
    snap.varVals = A.varVals;
    A.undoSnapshots.push_back(snap);
    if ((int)A.undoSnapshots.size() > A.undoMax) {
        freeTree(A.undoSnapshots.front().root);
        A.undoSnapshots.erase(A.undoSnapshots.begin());
    }
}

//回到上一个状态
static void DoUndo(AppState& A) {
    if (A.undoSnapshots.empty()) {
        A.status = "撤销：没有可撤销的操作";
        return;
    }
	AppState::UndoSnapshot snap = A.undoSnapshots.back();   // 取出最后一个快照
    A.undoSnapshots.pop_back();
    if (A.cur.root) freeTree(A.cur.root);
    A.cur.root = snap.root;
    A.varVals = snap.varVals;
    A.hasCur = (A.cur.root != nullptr);
    A.selectedNode = nullptr;
    rebuildLayout(A);
    refreshVars(A);
    A.status = "撤销完成";
}

// 清空撤销栈
static void clearUndo(AppState& A) {
    for (auto& s : A.undoSnapshots) freeTree(s.root);
    A.undoSnapshots.clear();
}


// ===================== 树上点选功能 =====================
static Node* HitTestNode(const Layout& L, int mx, int my, double zoom, int centerX, int centerY,
    int offsetX = 0, int offsetY = 0, int R = 18) {
    int bestD2 = (int)1e9;
    Node* best = nullptr;

    int scaledR = (int)(R * zoom);
    if (scaledR < 8) scaledR = 8;

    for (auto& kv : L.pos) {
        Node* p = kv.first;
        // 计算缩放后的坐标 + 偏移量
        int sx = centerX + (int)((kv.second.x - centerX) * zoom) + offsetX;
        int sy = centerY + (int)((kv.second.y - centerY) * zoom) + offsetY;

        int dx = mx - sx, dy = my - sy;
        int d2 = dx * dx + dy * dy;
        if (d2 <= scaledR * scaledR && d2 < bestD2) {
            bestD2 = d2;
            best = p;
        }
    }
    return best;
}
// 在树中查找某节点的父节点
static bool FindParent(Node* root, Node* target, Node*& parent, bool& isLeft) {
    if (!root) return false;
    if (root->l == target) { parent = root; isLeft = true; return true; }
    if (root->r == target) { parent = root; isLeft = false; return true; }
    return FindParent(root->l, target, parent, isLeft) || FindParent(root->r, target, parent, isLeft);
}

// 将选中的节点包装为函数调用节点
static bool WrapSelectedAsFunc(ExprTree& T, Node* selected, const std::string& funcName) {
    if (!T.root || !selected) return false;
    Node* f = new Node();
    f->kind = 'F';
    f->ch = funcCodeFromName(funcName);
    f->l = selected;
    f->r = nullptr;
    if (selected == T.root) {
        T.root = f;
        return true;
    }
    Node* parent = nullptr;
    bool isLeft = true;
    if (!FindParent(T.root, selected, parent, isLeft) || !parent) return false;
    if (isLeft) parent->l = f;
    else parent->r = f;
    return true;
}

// ===================== 界面绘制辅助 =====================
static void refreshVars(AppState& A) {
    A.varsInCur.clear();
    A.varList.clear();
    A.selectedVarIdx = -1;
    if (!A.hasCur) return;
    A.varsInCur = A.cur.collectVars();
    for (char c : A.varsInCur) A.varList.push_back(c);
}

// 重建布局
static void rebuildLayout(AppState& A) {
    if (!A.hasCur) { A.lay.pos.clear(); return; }
    int rx = A.leftW;
    int rw = A.W - A.leftW;

    const int titleH = 60;
    const int infoH = 180;
    const int statusH = 45;
    int treeH = A.H - statusH - titleH - infoH;

    int x0 = rx + (A.W - rx) / 2;
    int y0 = 300;
    int xGap = 60;
    int yGap = 80;

    // 传递树区域的最大宽度和高度
    int maxTreeWidth = rw - 60;
    int maxTreeHeight = treeH - 100;

    A.lay = layoutTree(A.cur.root, x0, y0, xGap, yGap, maxTreeWidth, maxTreeHeight);
}

// ===================== 变量面板绘制 =====================
static void drawVarPanel(const AppState& A) {
    const RectI& r = A.varPanel;
    setfillcolor(RGB(248, 248, 248));
    solidrectangle(r.x, r.y, r.x + r.w, r.y + r.h);
    setlinecolor(RGB(210, 210, 210));
    rectangle(r.x, r.y, r.x + r.w, r.y + r.h);

    setbkmode(TRANSPARENT);
    settextstyle(FONT_SMALL, 0, L"Microsoft YaHei");
    settextcolor(RGB(40, 40, 40));
    outtextxy(r.x + 10, r.y + 8, L"变量赋值：点选变量 -> 点\"赋值\"");

    int lineY = r.y + 38;
    int lineH = 26;
    for (int i = 0; i < (int)A.varList.size(); ++i) {
        int yy = lineY + i * lineH;
        if (yy + lineH > r.y + r.h - 8) break;
        bool sel = (i == A.selectedVarIdx);
        if (sel) {
            setfillcolor(RGB(210, 225, 255));
            solidrectangle(r.x + 8, yy - 2, r.x + r.w - 8, yy + lineH - 2);
        }
        char v = A.varList[i];
        string row;
        row.push_back(v);
        row += " = ";
        auto it = A.varVals.find(v);
        if (it == A.varVals.end()) row += "<未赋值>";
        else row += fmtDouble(it->second);
        outtextxy(r.x + 12, yy, s2ws(row).c_str());
    }
}

// 根据鼠标位置获取变量列表项索引，未命中返回 -1
static int varItemAt(const AppState& A, int mx, int my) {
    const RectI& r = A.varPanel;
    int lineY = r.y + 38;
    int lineH = 26;
    if (my < lineY) return -1;
    int idx = (my - lineY) / lineH;
    if (idx < 0 || idx >= (int)A.varList.size()) return -1;
    return idx;
}

// ===================== 业务操作 =====================
// 解析输入并建树
static void doBuild(AppState& A) {
    A.status.clear();
    A.hasCur = false;
    A.cur.clear();
    A.selectedNode = nullptr;

    string err;
    if (!A.cur.buildFromPostfixChars(A.tbInput.text, &err)) {
        A.status = "解析/建树失败：" + err;
        return;
    }
    A.hasCur = true;
    refreshVars(A);
    rebuildLayout(A);
    A.status = "建树成功：变量数=" + std::to_string((int)A.varsInCur.size());
}

// 显示中缀表达式
static void doShowInfix(AppState& A) {
    if (!A.hasCur) { A.status = "请先解析/建树"; return; }
    string s = A.cur.toInfix();
    MessageBoxW(GetHWnd(), s2ws(s).c_str(), L"一般数学表达式（中缀+括号）", MB_OK);
    A.status = "已输出中缀表达式";
}

// doAssignVar函数：给选中的变量赋值
static void doAssignVar(AppState& A) {
    if (!A.hasCur) { A.status = "请先解析/建树"; return; }
    if (A.selectedVarIdx < 0 || A.selectedVarIdx >= (int)A.varList.size()) {
        A.status = "请先在变量列表点选一个变量";
        return;
    }
    char v = A.varList[A.selectedVarIdx];
    double val = 0;
    string title = string("变量 ") + v + " 赋值";
    if (ModalInputNumber(title, "例如：3.14 或 -2", val)) {
        PushUndo(A);  // ★赋值前先保存快照
        A.varVals[v] = val;
        A.status = string("已设置 ") + v + " = " + fmtDouble(val);
    }
    else {
        A.status = "取消赋值";
    }
}

// 求值当前表达式
static void doEval(AppState& A) {
    if (!A.hasCur) { A.status = "请先解析/建树"; return; }
    double out = 0; string err;
    if (!A.cur.eval(A.varVals, out, &err)) {
        A.status = "求值失败：" + err;
        return;
    }
    A.lastValue = out;
    A.status = "求值成功：值 = " + fmtDouble(out);
}

// 保存当前表达式到槽位
static void doSaveCurToSlot(AppState& A) {
    if (!A.hasCur) { A.status = "请先解析/建树"; return; }

    int idx = -1;
    if (!ModalPickSlot((int)A.slots.size(), L"保存当前表达式到槽位", idx)) {
        A.status = "取消保存";
        return;
    }

    A.slots[idx].clear();
    // ★使用 substituteVars 把已赋值的变量替换为常量
    A.slots[idx].root = substituteVars(A.cur.root, A.varVals);
    A.slots[idx].updateCaches();  // ★重新生成后缀和中缀
    A.hasSlot[idx] = true;

    A.status = "已保存当前表达式到 槽位" + std::to_string(idx + 1);
}

// 求偏导并保存到槽位
static void doDerivativeToSlot(AppState& A) {
    if (!A.hasCur) { A.status = "请先解析/建树"; return; }

    refreshVars(A);
    if (A.varList.empty()) {
        MessageBoxW(GetHWnd(), L"表达式中没有变量，偏导结果为 0。", L"提示", MB_OK);
        ExprTree D; D.root = makeNum(0);

        int idx = -1;
        if (!ModalPickSlot((int)A.slots.size(), L"将偏导结果保存到槽位", idx)) {
            A.status = "偏导完成（未保存）";
            freeTree(D.root);
            return;
        }
        A.slots[idx].clear();
        A.slots[idx].root = D.root; D.root = nullptr;
        A.slots[idx].postfixRaw = "<derivative>";
        A.hasSlot[idx] = true;
        A.status = "偏导结果已保存到 槽位" + std::to_string(idx + 1);
        return;
    }

    char v = A.varList[0];
    if (!ModalPickVar(A.varList, v)) { A.status = "取消求偏导"; return; }

    string err;
    ExprTree D = DerivativeTree(A.cur, v, &err);
    if (!D.root) {
        MessageBoxW(GetHWnd(), s2ws("求偏导失败：" + err).c_str(), L"错误", MB_OK);
        A.status = "求偏导失败：" + err;
        return;
    }

    int idx = -1;
    if (!ModalPickSlot((int)A.slots.size(), L"将偏导结果保存到槽位", idx)) {
        A.status = "偏导完成（未保存）";
        D.clear();
        return;
    }

    // 在 doDerivativeToSlot 函数中，保存结果时：
    A.slots[idx].clear();
    A.slots[idx].root = D.root; D.root = nullptr;
    A.slots[idx].updateCaches();  // 生成后缀和中缀
    A.hasSlot[idx] = true;

    MessageBoxW(GetHWnd(), s2ws("偏导完成并保存到 槽位" + std::to_string(idx + 1) +
        "\n\n结果（中缀+括号）：\n" + A.slots[idx].toInfix()).c_str(),
        L"偏导结果已保存", MB_OK);

    A.status = "偏导结果已保存到 槽位" + std::to_string(idx + 1);
}

// 从槽位构造复合表达式（一步弹窗版本）
static void doComposeFromSlotsBest(AppState& A) {
    // 至少要有两个非空槽位
    int nonEmpty = 0;
    for (int i = 0; i < (int)A.hasSlot.size(); ++i)
        if (A.hasSlot[i] && A.slots[i].root) nonEmpty++;
    if (nonEmpty < 2) { A.status = "至少需要两个非空槽位才能复合构造"; return; }

    int i = -1, j = -1; char op = 0;
    if (!ModalComposeBest(A.hasSlot, A.slots, i, j, op)) {
        A.status = "取消复合构造";
        return;
    }

    string err;
    ExprTree R = Compose(A.slots[i], A.slots[j], op, &err);
    if (!R.root) { A.status = "构造失败：" + err; return; }

    A.cur.clear();
    A.cur.root = R.root; R.root = nullptr;
    A.cur.postfixRaw = R.postfixRaw;
    A.hasCur = true;
    A.selectedNode = nullptr;  // 清空选中

    refreshVars(A);
    rebuildLayout(A);
    A.status = "已构造：(槽位" + std::to_string(i + 1) + ")" + op + "(槽位" + std::to_string(j + 1) + ")";
}

// 包裹选中节点为三角函数
static void doWrapFunc(AppState& A, const std::string& fn) {
    if (!A.hasCur || !A.cur.root) { A.status = "请先解析/建树"; return; }
    if (!A.selectedNode) { A.status = "请先在树上点击选中一个子表达式"; return; }

    PushUndo(A);  // 关键：修改前先存一份快照

    if (!WrapSelectedAsFunc(A.cur, A.selectedNode, fn)) {
        A.status = "包裹失败：未能定位被选节点";
        return;
    }

    A.selectedNode = nullptr;
    rebuildLayout(A);
    refreshVars(A);
    A.status = "已包裹为 " + fn + "(...)";
}

// 清空所有状态
static void doClear(AppState& A) {
    A.tbInput.clear();
    A.cur.clear();
    for (auto& s : A.slots) s.clear();
    for (int i = 0; i < (int)A.hasSlot.size(); i++) A.hasSlot[i] = false;
    A.hasCur = false;
    A.varsInCur.clear();
    A.varList.clear();
    A.selectedVarIdx = -1;
    A.varVals.clear();
    A.lastValue = 0;
    A.lay.pos.clear();
    A.selectedNode = nullptr;
    A.treeZoom = 1.0;       // 重置缩放
    A.treeOffsetX = 0;      // 重置偏移
    A.treeOffsetY = 0;
    A.treeDragging = false;
    A.status = "已清空（含变量赋值）";
}


// ===================== 按钮滚动布局 =====================

// 构建功能按钮
static void buildFuncButtons(AppState& A) {
    A.funcBtns.clear();

    auto add = [&](const string& txt) {
        Button b; b.text = txt; A.funcBtns.push_back(b);
    };

    add("1. 解析/建树");
    add("2. 输出正常表达式（中缀+括号）");
    add("3. 变量赋值（先点变量）");
    add("4. 求值");
    add("5. 保存当前表达式到槽位");
    add("6. 一步构造复合表达式 (E1)P(E2)");
    add("7. 求偏导并保存到槽位");
    add("8. 包裹 sin（先点树节点）");
    add("9. 包裹 cos（先点树节点）");
    add("10. 包裹 tan（先点树节点）");
    add("11. 槽位化简（选源/选目标）");
    add("12. 更新树状态（当前/槽位）");  // ★新增
    add("撤销（Undo）");
    add("清空");
    add("F11 全屏/窗口切换");
}

// 计算按钮布局
static void layoutFuncButtons(AppState& A) {
    int x = A.funcView.x;
    int y = A.funcView.y + A.scrollY;
    int w = A.funcView.w;
    int bh = 38, gap = 8;

    for (auto& b : A.funcBtns) {
        b.rc = { x, y, w, bh };
        y += bh + gap;
    }

    int contentH = (int)A.funcBtns.size() * (bh + gap) - gap;
    int viewH = A.funcView.h;

    if (contentH <= viewH) {
        A.scrollY = 0;
        A.scrollMin = 0;
        A.scrollMax = 0;
    }
    else {
        A.scrollMax = 0;
        A.scrollMin = viewH - contentH;
        if (A.scrollY > A.scrollMax) A.scrollY = A.scrollMax;
        if (A.scrollY < A.scrollMin) A.scrollY = A.scrollMin;
        x = A.funcView.x;
        y = A.funcView.y + A.scrollY;
        for (auto& b : A.funcBtns) {
            b.rc = { x, y, w, bh };
            y += bh + gap;
        }
    }
}

static void toggleFullscreen(AppState& A, bool& isFull);
static void RebuildViewLayout(AppState& A);

// 滚动功能按钮区
static void scrollFunc(AppState& A, int delta) {
    A.scrollY -= delta;
    if (A.scrollY > A.scrollMax) A.scrollY = A.scrollMax;
    if (A.scrollY < A.scrollMin) A.scrollY = A.scrollMin;
    layoutFuncButtons(A);
}

// ===================== 树视图功能 =====================

// 获取"当前显示树"的 root
static Node* GetViewRoot(const AppState& A) {
    if (A.viewTreeIdx == -1) {
        return (A.hasCur ? A.cur.root : nullptr);
    }
    int i = A.viewTreeIdx;
    if (i >= 0 && i < (int)A.slots.size() && A.hasSlot[i]) {
        return A.slots[i].root;
    }
    return nullptr;
}

// 重建右侧树布局
static void RebuildViewLayout(AppState& A) {
    int rx = A.leftW;
    int rw = A.W - A.leftW;

    const int titleH = 60;
    const int infoH = 180;
    const int statusH = 45;

    int treeY = titleH + infoH;
    int treeH = A.H - statusH - treeY;

    Node* root = GetViewRoot(A);

    int x0 = rx + rw / 2;
    int y0 = treeY + 60;
    int xGap = 60;
    int yGap = 80;

    // 传递树区域的最大宽度和高度
    int maxTreeWidth = rw - 60;      // 留出左右边距
    int maxTreeHeight = treeH - 100; // 留出上下边距

    A.viewLay = layoutTree(root, x0, y0, xGap, yGap, maxTreeWidth, maxTreeHeight);
}

// ===================== 绘制 =====================

//绘制表达式树，支持选中节点高亮显示和变量赋值显示
static void drawTreeWithSelect(Node* root, const Layout& L, int clipX, int clipY, int clipW, int clipH,
    Node* selected, const std::map<char, double>& varVals,  
    double zoom = 1.0, int offsetX = 0, int offsetY = 0) {
    if (!root) {
        settextstyle(FONT_NORMAL, 0, L"Microsoft YaHei");
        settextcolor(RGB(120, 120, 120));
        outtextxy(clipX + 18, clipY + 100, L"树为空：请先解析/建树");
        return;
    }

    int centerX = clipX + clipW / 2;
    int centerY = clipY + clipH / 2;

    auto scalePos = [&](int x, int y, int& sx, int& sy) {
        sx = centerX + (int)((x - centerX) * zoom) + offsetX;
        sy = centerY + (int)((y - centerY) * zoom) + offsetY;
        };

    //显示变量的赋值
    auto labelOf = [&](Node* p)->std::wstring {
        if (!p) return L"";
        if (p->kind == 'N') {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%.6g", p->num);
            return s2ws(buf);
        }
        if (p->kind == 'V') {
            // 如果变量已赋值，显示 "变量名=值"
            auto it = varVals.find(p->ch);
            if (it != varVals.end()) {
                char buf[48];
                std::snprintf(buf, sizeof(buf), "%c=%.4g", p->ch, it->second);
                return s2ws(buf);
            }
            return s2ws(std::string(1, p->ch));
        }
        if (p->kind == 'F') {
            std::string fn = funcNameFromCode(p->ch);
            return s2ws(fn);
        }
        return s2ws(std::string(1, p->ch));
        };

    int R = (int)(18 * zoom);
    if (R < 8) R = 8;

    // 先画边
    setlinecolor(RGB(120, 120, 120));
    std::function<void(Node*)> edge = [&](Node* p) {
        if (!p) return;
        auto itp = L.pos.find(p);
        if (itp == L.pos.end()) return;

        int x1, y1;
        scalePos(itp->second.x, itp->second.y, x1, y1);

        if (p->l) {
            auto it = L.pos.find(p->l);
            if (it != L.pos.end()) {
                int x2, y2;
                scalePos(it->second.x, it->second.y, x2, y2);
                line(x1, y1, x2, y2);
            }
        }
        if (p->r) {
            auto it = L.pos.find(p->r);
            if (it != L.pos.end()) {
                int x2, y2;
                scalePos(it->second.x, it->second.y, x2, y2);
                line(x1, y1, x2, y2);
            }
        }
        edge(p->l); edge(p->r);
        };
    edge(root);

    // 再画点
    int fontSize = (int)(FONT_NORMAL * zoom);
    if (fontSize < 12) fontSize = 12;
    if (fontSize > 36) fontSize = 36;
    settextstyle(fontSize, 0, L"Microsoft YaHei");

    std::function<void(Node*)> node = [&](Node* p) {
        if (!p) return;
        auto it = L.pos.find(p);
        if (it == L.pos.end()) return;

        int x, y;
        scalePos(it->second.x, it->second.y, x, y);

        // 变量已赋值时用不同颜色
        bool isAssignedVar = (p->kind == 'V' && varVals.find(p->ch) != varVals.end());

        if (p == selected) {
            setfillcolor(RGB(255, 245, 200));
            setlinecolor(RGB(220, 120, 20));
        }
        else if (isAssignedVar) {
            setfillcolor(RGB(220, 255, 220));  // 浅绿色表示已赋值
            setlinecolor(RGB(60, 160, 60));
        }
        else {
            setfillcolor(WHITE);
            setlinecolor(RGB(80, 80, 80));
        }

        // 变量已赋值时节点画大一点
        int nodeR = isAssignedVar ? (int)(R * 1.3) : R;
        solidcircle(x, y, nodeR);
        circle(x, y, nodeR);

        std::wstring t = labelOf(p);
        setbkmode(TRANSPARENT);
        settextcolor(isAssignedVar ? RGB(0, 100, 0) : RGB(20, 20, 20));
        outtextxy(x - textwidth(t.c_str()) / 2, y - textheight(t.c_str()) / 2, t.c_str());

        node(p->l); node(p->r);
        };
    node(root);
}
// ===================== 槽位化简功能 =====================
// 显示模态输入对话框，输入数字
static bool ModalInputInt(const std::string& title, const std::string& hint, int& outVal) {
    double tmp = 0;
    if (!ModalInputNumber(title, hint, tmp)) return false;
    outVal = (int)tmp;
    return true;
}

// 选择槽位索引，允许选择当前表达式（返回 -1），取消返回 -999999
static int pickSlotIndex(AppState& A, const std::string& title, bool allowCurrent) {
    int N = (int)A.slots.size();
    int k = 0;

    std::string hint = allowCurrent
        ? ("输入 0=当前表达式，1.." + std::to_string(N) + "=槽位")
        : ("输入 1.." + std::to_string(N) + "=槽位");

    if (!ModalInputInt(title, hint, k)) return -999999;

    if (allowCurrent) {
        if (k == 0) return -1;
        if (k >= 1 && k <= N) return k - 1;
    }
    else {
        if (k >= 1 && k <= N) return k - 1;
    }
    MessageBoxW(GetHWnd(), L"槽位编号不合法。", L"提示", MB_OK);
    return -999999;
}

// 更新树状态
static void doUpdateTreeState(AppState& A) {
    int N = (int)A.slots.size();
    int k = 0;

    std::string hint = "输入 0=当前表达式树，1.." + std::to_string(N) + "=槽位树";
    if (!ModalInputInt("更新树状态", hint, k)) {
        A.status = "取消更新树状态";
        return;
    }

    if (k == 0) {
        if (!A.hasCur || !A.cur.root) { 
            A.status = "当前表达式为空，无法显示"; 
            return; 
        }
        A.viewTreeIdx = -1;
    } else {
        int idx = k - 1;
        if (idx < 0 || idx >= N || !A.hasSlot[idx] || !A.slots[idx].root) {
            A.status = "该槽位为空，无法显示";
            return;
        }
        A.viewTreeIdx = idx;
    }

    A.selectedNode = nullptr;
    RebuildViewLayout(A);

    A.status = (A.viewTreeIdx == -1)
        ? "树状态已更新：当前表达式树"
        : ("树状态已更新：槽位 " + std::to_string(A.viewTreeIdx + 1) + " 的树");
}

// ===================== 绘制主界面 =====================
// 绘制面板背景
static void drawApp(AppState& A) {
    BeginBatchDraw();
    cleardevice();
    setbkmode(TRANSPARENT);

    // ===== 左侧 =====
    drawPanelBg(0, 0, A.leftW, A.H);

    const int leftPad = 18;
    const int titleY = 16;
    int inputLabelY = A.tbInput.rc.y - 25;
    int funcLabelY = A.funcView.y - 38;

    // 先绘制功能区背景
    setfillcolor(RGB(245, 245, 245));
    solidrectangle(A.funcView.x - 2, A.funcView.y - 2, A.funcView.x + A.funcView.w + 2, A.funcView.y + A.funcView.h + 2);

    // 绘制按钮（可能溢出）
    for (const auto& b : A.funcBtns) {
        // 只绘制与可视区域有交集的按钮
        if (b.rc.y + b.rc.h < A.funcView.y) continue;
        if (b.rc.y > A.funcView.y + A.funcView.h) continue;
        b.draw();
    }

    // 用遮罩覆盖溢出的按钮内容
    setfillcolor(RGB(248, 248, 248));
    // 上方遮罩
    solidrectangle(0, 0, A.leftW, A.funcView.y - 1);
    // 下方遮罩
    solidrectangle(0, A.funcView.y + A.funcView.h + 3, A.leftW, A.varPanel.y - 1);

    // 重绘功能区边框
    setlinecolor(RGB(210, 210, 210));
    rectangle(A.funcView.x - 2, A.funcView.y - 2, A.funcView.x + A.funcView.w + 2, A.funcView.y + A.funcView.h + 2);

    // 重绘被遮罩覆盖的标题和标签
    settextstyle(FONT_TITLE, 0, L"Microsoft YaHei");
    settextcolor(RGB(30, 30, 30));
    outtextxy(leftPad, titleY, L"后缀表达式（界面版）");

    settextstyle(FONT_NORMAL, 0, L"Microsoft YaHei");
    outtextxy(leftPad, inputLabelY, L"输入后缀表达式：");
    A.tbInput.draw("例：ab+c*   或   23+5*");

    settextstyle(FONT_SMALL, 0, L"Microsoft YaHei");
    outtextxy(leftPad, funcLabelY, L"功能选择（滚轮可滑动）：");

    drawVarPanel(A);
    // ===== 右侧 =====
    int rx = A.leftW;
    int rw = A.W - A.leftW;

    drawPanelBg(rx, 0, rw, A.H);

    const int pad = 18;
    const int titleH = 60;
    const int infoH = 180;
    const int statusH = 45;

    // ---- ① 标题区 ----
    drawPanelBg(rx, 0, rw, titleH);
    settextstyle(FONT_TITLE, 0, L"Microsoft YaHei");
    settextcolor(RGB(30, 30, 30));
    outtextxy(rx + pad, 18, L"结果展示区");

    // ---- ② 右侧结果区 ----
    int infoY = titleH;
    drawPanelBg(rx, infoY-5, rw, infoH+5);
    settextstyle(FONT_NORMAL, 0, L"Microsoft YaHei");
    settextcolor(RGB(40, 40, 40));

    std::vector<std::string> lines;
    lines.push_back("当前表达式（中缀）：" + (A.hasCur ? A.cur.toInfix() : "<无>"));
    lines.push_back("当前表达式（后缀）：" + (A.hasCur ? A.cur.postfixRaw : "<无>"));
    lines.push_back("");
    lines.push_back("最近求值结果： " + fmtDouble(A.lastValue));
    lines.push_back("");
    lines.push_back("槽位列表：");
    for (int i = 0; i < (int)A.slots.size(); ++i) {
        std::string s = "  槽位" + std::to_string(i + 1) + "：";
        if (A.hasSlot[i]) {
            // 中缀和后缀放在同一行
            s += "中缀:    " + A.slots[i].toInfix() + "                            "+ "后缀:     " + A.slots[i].postfixRaw;
        }
        else {
            s += "<空>";
        }
        lines.push_back(s);
    }

    const int lineH = 24;
    const int topPad = 10;
    const int bottomPad = 10;

    int contentH = (int)lines.size() * lineH;
    int viewH = infoH - topPad - bottomPad;

    const int maxY = 0;
    int minY = viewH - contentH;     // content 比视口高时 minY 为负数
    if (minY > 0) minY = 0;          // content 比视口矮时，不允许滚动，minY 固定 0

    A.rightScrollMin = minY;

    // 关键：当不需要滚动时，直接锁死为 0（防止上滑“飘”）
    if (contentH <= viewH) {
        A.rightScrollY = 0;
    }
    else {
        if (A.rightScrollY < minY) A.rightScrollY = minY;
        if (A.rightScrollY > maxY) A.rightScrollY = maxY;
    }


    int baseY = infoY + topPad + A.rightScrollY;
    for (int i = 0; i < (int)lines.size(); ++i) {
        int yy = baseY + i * lineH;
        if (yy < infoY + topPad - lineH) continue;
        if (yy > infoY + infoH - bottomPad) break;
        outtextxy(rx + pad, yy, s2ws(lines[i]).c_str());
    }

    settextstyle(FONT_SMALL, 0, L"Microsoft YaHei");
    settextcolor(RGB(120, 120, 120));
    outtextxy(rx + rw - 220, infoY + infoH - 28, L"滚轮：滚动结果区");

    if (contentH > viewH) {
        int barX = rx + rw - 10;
        int barY0 = infoY + topPad;
        int barH = viewH;

        setfillcolor(RGB(230, 230, 230));
        solidrectangle(barX, barY0, barX + 4, barY0 + barH);

        double ratio = (double)viewH / (double)contentH;
        int thumbH = (int)(barH * ratio);
        if (thumbH < 18) thumbH = 18;

        double t = (A.rightScrollMin == 0) ? 0.0 : (double)(-A.rightScrollY) / (double)(-A.rightScrollMin);
        int thumbY = barY0 + (int)((barH - thumbH) * t);

        setfillcolor(RGB(160, 160, 160));
        solidrectangle(barX, thumbY, barX + 4, thumbY + thumbH);
    }

    // ---- ④ 状态栏 ----
    int statusY = A.H - statusH;
    drawPanelBg(rx, statusY, rw, statusH);
    settextstyle(FONT_NORMAL, 0, L"Microsoft YaHei");
    settextcolor(RGB(60, 60, 60));
    outtextxy(rx + pad, statusY + 12, s2ws("状态： " + A.status).c_str());

    // ---- ③ 树绘图区 ----
    int treeY = infoY + infoH;
    int treeH = statusY - treeY;
    drawPanelBg(rx, treeY, rw, treeH);

    settextstyle(FONT_NORMAL, 0, L"Microsoft YaHei");
    settextcolor(RGB(40, 40, 40));
    
    // 显示当前树视图来源
    std::wstring treeTitle = L"表达式二叉树";
    if (A.viewTreeIdx == -1) {
        treeTitle += L"（当前表达式）";
    } else {
        treeTitle += L"（槽位" + std::to_wstring(A.viewTreeIdx + 1) + L"）";
    }
    treeTitle += L" - 点击节点可选中";
    outtextxy(rx + pad, treeY + 10, treeTitle.c_str());
    // 使用 viewLay 和 GetViewRoot 绘制树（传入缩放比例和偏移量）

    Node* viewRoot = GetViewRoot(A);
    drawTreeWithSelect(viewRoot, A.viewLay, rx, treeY + 40, rw, treeH - 40,
        A.selectedNode, A.varVals,  // ★传入变量值映射
        A.treeZoom, A.treeOffsetX, A.treeOffsetY);

    // 显示缩放比例和拖动提示
    settextstyle(FONT_SMALL, 0, L"Microsoft YaHei");
    settextcolor(RGB(120, 120, 120));
    char zoomBuf[64];
    std::snprintf(zoomBuf, sizeof(zoomBuf), "缩放: %.0f%% | 拖动: 按住拖拽", A.treeZoom * 100);
    outtextxy(rx + rw - 200, treeY + 10, s2ws(zoomBuf).c_str());

	FlushBatchDraw();  // 提交绘制

}

// 槽位化简
static void doSimplifySlotToSlot(AppState& A) {
    int src = pickSlotIndex(A, "选择要化简的表达式（源）", true);
    if (src == -999999) { A.status = "取消化简"; return; }

    int dst = pickSlotIndex(A, "选择保存化简结果的槽位（目标）", false);
    if (dst == -999999) { A.status = "取消化简"; return; }

    ExprTree tmp;
    if (src == -1) {
        if (!A.hasCur || !A.cur.root) { A.status = "当前表达式为空"; return; }
        tmp = A.cur.clone();
    }
    else {
        if (src < 0 || src >= (int)A.slots.size() || !A.hasSlot[src]) {
            A.status = "源槽位为空"; return;
        }
        tmp = A.slots[src].clone();
    }

    tmp.simplify();  // simplify 内部会调用 updateCaches()

    A.slots[dst].clear();
    A.slots[dst].root = tmp.root;
    A.slots[dst].postfixRaw = tmp.postfixRaw;  // 使用更新后的后缀
    A.slots[dst].infixCache = tmp.infixCache;  // 使用更新后的中缀
    tmp.root = nullptr;
    A.hasSlot[dst] = true;

    RebuildViewLayout(A);

    std::string srcName = (src == -1) ? "当前表达式" : ("槽位" + std::to_string(src + 1));
    A.status = "化简完成：" + srcName + " -> 槽位" + std::to_string(dst + 1);
}

static void toggleFullscreen(AppState& A, bool& isFull) {
    isFull = !isFull;
    closegraph();

    // 记录原始窗口尺寸（用于计算缩放比例）
    const int BASE_W = 1200;
    const int BASE_H = 720;

    if (isFull) {
        initgraph(GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), EW_SHOWCONSOLE);
        // 无边框全屏效果
        HWND hwnd = GetHWnd();
        SetWindowLong(hwnd, GWL_STYLE, GetWindowLong(hwnd, GWL_STYLE) & ~WS_CAPTION);
        SetWindowPos(hwnd, HWND_TOP, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), SWP_SHOWWINDOW);
        A.W = getwidth();
        A.H = getheight();
    }
    else {
        A.W = BASE_W;
        A.H = BASE_H;
        initgraph(A.W, A.H, EW_SHOWCONSOLE);
    }

    // 计算缩放比例
    double scaleX = (double)A.W / BASE_W;
    double scaleY = (double)A.H / BASE_H;
    double scale = (std::min)(scaleX, scaleY);  // 使用较小的比例，保持比例不变形

    // 按比例缩放左侧宽度
    A.leftW = (int)(420 * scale);

    // 按比例缩放各组件位置和尺寸
    int leftPad = (int)(18 * scale);
    int inputY = (int)(75 * scale);
    int inputH = (int)(38 * scale);
    int funcY = (int)(170 * scale);
    int funcH = (int)(220 * scale);
    int varY = (int)(400 * scale);
    int varH = (int)(220 * scale);

    A.tbInput.rc = { leftPad, inputY, A.leftW - leftPad * 2, inputH };
    A.funcView = { leftPad, funcY, A.leftW - leftPad * 2, funcH };
    A.varPanel = { leftPad, varY, A.leftW - leftPad * 2, varH };

    layoutFuncButtons(A);
    rebuildLayout(A);
    RebuildViewLayout(A);
}

// ===================== 主程序 =====================
int main() {
    AppState A;

    A.slots.assign(SLOT_N, ExprTree());
    A.hasSlot.assign(SLOT_N, false);

    initgraph(A.W, A.H, EW_SHOWCONSOLE);
    settextstyle(FONT_NORMAL, 0, L"Microsoft YaHei");

    // 使用与 toggleFullscreen 相同的基准布局
    const int leftPad = 18;
    A.leftW = 420;

    A.tbInput.rc = { leftPad, 75, A.leftW - leftPad * 2, 38 };
    A.tbInput.active = false;

    A.funcView = { leftPad, 170, A.leftW - leftPad * 2, 220 };
    A.varPanel = { leftPad, 400, A.leftW - leftPad * 2, 220 };

    buildFuncButtons(A);
    layoutFuncButtons(A);

    // 初始化视图状态
    A.viewTreeIdx = -1;
    RebuildViewLayout(A);

    A.status = "就绪：输入后缀字符序列，然后点击\"解析/建树\"。";

    bool full = false;
    ExMessage msg{};

    while (true) {
        POINT pt;
        GetCursorPos(&pt);
        ScreenToClient(GetHWnd(), &pt);
        int mx = pt.x, my = pt.y;

        // 只有在 funcView 范围内才显示悬停效果
        for (auto& b : A.funcBtns) {
            if (hitRect(A.funcView, mx, my)) {
                b.hot = b.hit(mx, my);
            }
            else {
                b.hot = false;
            }
        }

        drawApp(A);

        while (peekmessage(&msg, EM_MOUSE | EM_KEY | EM_CHAR)) {
            if (msg.message == WM_MOUSEWHEEL) {
                int rx = A.leftW;
                int rw = A.W - A.leftW;
                const int titleH = 60;
                const int infoH = 180;
                const int statusH = 45;

                RectI rightInfoRect = { rx, titleH, rw, infoH };

                // 树区域
                int treeY = titleH + infoH;
                int treeH = A.H - statusH - treeY;
                RectI treeRect = { rx, treeY, rw, treeH };

                if (hitRect(rightInfoRect, msg.x, msg.y)) {
                    // 右侧信息区滚动
                    int step = 44;
                    if ((int)msg.wheel > 0) A.rightScrollY += step;
                    else A.rightScrollY -= step;

                    if (A.rightScrollY > 0) A.rightScrollY = 0;
                    if (A.rightScrollY < A.rightScrollMin) A.rightScrollY = A.rightScrollMin;
                }
                else if (hitRect(treeRect, msg.x, msg.y)) {
                    // 树区域缩放
                    double zoomStep = 0.15;
                    if ((int)msg.wheel > 0) {
                        A.treeZoom += zoomStep;
                        if (A.treeZoom > A.ZOOM_MAX) A.treeZoom = A.ZOOM_MAX;
                    }
                    else {
                        A.treeZoom -= zoomStep;
                        if (A.treeZoom < A.ZOOM_MIN) A.treeZoom = A.ZOOM_MIN;
                    }
                    A.status = "缩放: " + std::to_string((int)(A.treeZoom * 100)) + "%";
                }
                else if (hitRect(A.funcView, msg.x, msg.y)) {
                    // 左侧按钮区滚动
                    int delta = (int)msg.wheel;
                    if (delta > 0) scrollFunc(A, -40);
                    else if (delta < 0) scrollFunc(A, 40);
                }
            }
            else if (msg.message == WM_LBUTTONDOWN) {
                // 输入框鼠标按下
                if (A.tbInput.hit(msg.x, msg.y)) {
                    A.tbInput.activate();
                    A.tbInput.onMouseDown(msg.x, msg.y);
                }
                else {
                    A.tbInput.active = false;
                    A.tbInput.clearSelection();
                }

                if (hitRect(A.varPanel, msg.x, msg.y)) {
                    int idx = varItemAt(A, msg.x, msg.y);
                    if (idx != -1) A.selectedVarIdx = idx;
                }

                // 只有在 funcView 范围内才响应按钮按下
                if (hitRect(A.funcView, msg.x, msg.y)) {
                    for (auto& b : A.funcBtns)
                        b.down = b.hit(msg.x, msg.y);
                }

                // 检测树区域的拖动开始
                {
                    int rx = A.leftW;
                    const int titleH = 60;
                    const int infoH = 180;
                    const int statusH = 45;
                    int treeY = titleH + infoH;
                    int treeH = A.H - statusH - treeY;
                    int rw = A.W - A.leftW;

                    RectI treeRect = { rx, treeY, rw, treeH };
                    if (hitRect(treeRect, msg.x, msg.y)) {
                        A.treeDragging = true;
                        A.treeDragStartX = msg.x;
                        A.treeDragStartY = msg.y;
                    }
                }
            }
            else if (msg.message == WM_MOUSEMOVE) {
                // 输入框拖选
                if (A.tbInput.selecting) {
                    A.tbInput.onMouseMove(msg.x, msg.y);
                }
                // 处理树拖动
                if (A.treeDragging) {
                    int dx = msg.x - A.treeDragStartX;
                    int dy = msg.y - A.treeDragStartY;
                    A.treeOffsetX += dx;
                    A.treeOffsetY += dy;
                    A.treeDragStartX = msg.x;
                    A.treeDragStartY = msg.y;
                }
            }
            else if (msg.message == WM_LBUTTONUP) {
                // 输入框鼠标释放
                A.tbInput.onMouseUp(msg.x, msg.y);

                bool wasDragging = A.treeDragging;
                int dragDistX = std::abs(msg.x - A.treeDragStartX);
                int dragDistY = std::abs(msg.y - A.treeDragStartY);
                A.treeDragging = false;

                // 树上点选：只有在没有明显拖动时才选中节点
                if (!wasDragging || (dragDistX < 5 && dragDistY < 5)) {
                    int rx = A.leftW;
                    const int titleH = 60;
                    const int infoH = 180;
                    const int statusH = 45;
                    int treeY = titleH + infoH;
                    int treeH = A.H - statusH - treeY;
                    int rw = A.W - A.leftW;

                    RectI treeRect = { rx, treeY, rw, treeH };
                    if (hitRect(treeRect, msg.x, msg.y)) {
                        int centerX = rx + rw / 2;
                        int centerY = treeY + 40 + (treeH - 40) / 2;

                        Node* hit = HitTestNode(A.viewLay, msg.x, msg.y, A.treeZoom, centerX, centerY,
                            A.treeOffsetX, A.treeOffsetY);
                        if (hit) {
                            A.selectedNode = hit;
                            A.status = "已选中子表达式，可点击 sin/cos/tan 包裹";
                        }
                    }
                }
                else if (msg.message == WM_LBUTTONDBLCLK) {
                    // 双击选中全部
                    if (A.tbInput.hit(msg.x, msg.y)) {
                        A.tbInput.activate();
                        A.tbInput.onDoubleClick(msg.x, msg.y);
                    }
                }
                // 只有在 funcView 范围内才响应按钮点击
                

                auto clickedIndex = [&]() -> int {
                    if (!hitRect(A.funcView, msg.x, msg.y)) {
                        // 鼠标不在功能区内，清除所有按下状态并返回 -1
                        for (auto& b : A.funcBtns) b.down = false;
                        return -1;
                    }
                    for (int i = 0; i < (int)A.funcBtns.size(); ++i) {
                        bool c = A.funcBtns[i].down && A.funcBtns[i].hit(msg.x, msg.y);
                        A.funcBtns[i].down = false;
                        if (c) return i;
                    }
                    return -1;
                    };

                int idx = clickedIndex();
                if (idx != -1) {
                    switch (idx) {
                    case 0:  doBuild(A); RebuildViewLayout(A); break;
                    case 1:  doShowInfix(A); break;
                    case 2:  doAssignVar(A); break;
                    case 3:  doEval(A); break;
                    case 4:  doSaveCurToSlot(A); RebuildViewLayout(A); break;
                    case 5:  doComposeFromSlotsBest(A); RebuildViewLayout(A); break;
                    case 6:  doDerivativeToSlot(A); RebuildViewLayout(A); break;
                    case 7:  doWrapFunc(A, "sin"); RebuildViewLayout(A); break;
                    case 8:  doWrapFunc(A, "cos"); RebuildViewLayout(A); break;
                    case 9:  doWrapFunc(A, "tan"); RebuildViewLayout(A); break;
                    case 10: doSimplifySlotToSlot(A); break;
                    case 11: doUpdateTreeState(A); break;
                    case 12: DoUndo(A); RebuildViewLayout(A); break;
                    case 13: doClear(A); A.viewLay.pos.clear(); A.viewTreeIdx = -1; break;
                    case 14:
                        toggleFullscreen(A, full);
                        break;
                    }
                }
            }
            else if (msg.message == WM_CHAR) {
                A.tbInput.onChar((int)msg.ch);
            }
            else if (msg.message == WM_KEYDOWN) {
                //★获取 Shift 和 Ctrl 状态
                bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;

                // 输入框按键处理
                A.tbInput.onKeyDown(msg.vkcode, shift, ctrl);

                if (msg.vkcode == VK_ESCAPE) {
                    closegraph();
                    return 0;
                }
                if (msg.vkcode == VK_RETURN) {
                    doBuild(A);
                    RebuildViewLayout(A);
                }
                if (msg.vkcode == VK_F11) {
                    toggleFullscreen(A, full);
                }
                // 只有输入框未激活时，上下键才滚动按钮列表
                if (!A.tbInput.active) {
                    if (msg.vkcode == VK_UP)   scrollFunc(A, -40);
                    if (msg.vkcode == VK_DOWN) scrollFunc(A, 40);
                }
                }
        }
        Sleep(10);
    }
}
