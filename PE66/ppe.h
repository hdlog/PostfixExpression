#ifndef PPE_H
#define PPE_H

#include <string>
#include <vector>
#include <map>
#include <set>
#include <cmath>
#include <functional>
#include <sstream>

using std::string;
using std::vector;

// ===================== 表达式树节点 =====================
struct Node {
    char kind;    // 'N' 数字, 'V' 变量, 'O' 运算符, 'F' 一元函数
    char ch;      // 变量名/运算符/函数编码(s/c/t/l)
    double num;   // 数字
	Node* l;      // 左子节点
	Node* r;      // 右子节点
	Node() : kind('N'), ch(0), num(0), l(nullptr), r(nullptr) {} // 默认构造函数
};

// ===================== 内存管理函数 =====================

// 释放表达式树的所有节点
inline void freeTree(Node* p) {
    if (!p) return;
    freeTree(p->l);
    freeTree(p->r);
    delete p;
}

// 深拷贝一棵表达式树
inline Node* cloneTree(Node* p) {
    if (!p) return nullptr;
    Node* q = new Node();
    *q = *p;
    q->l = cloneTree(p->l);
    q->r = cloneTree(p->r);
    return q;
}

// ===================== 函数编码/解码（支持 sin/cos/tan/ln） =====================

// 函数名转编码：sin->s, cos->c, tan->t, ln->l
inline char funcCodeFromName(const std::string& name) {
    if (name == "sin") return 's';
    if (name == "cos") return 'c';
    if (name == "tan") return 't';
    if (name == "ln")  return 'l';
    return 0;
}

// 编码转函数名：s->sin, c->cos, t->tan, l->ln
inline std::string funcNameFromCode(char c) {
    if (c == 's') return "sin";
    if (c == 'c') return "cos";
    if (c == 't') return "tan";
    if (c == 'l') return "ln";
    return "func?";
}

// ===================== 节点创建工具函数 =====================

// 创建数字节点
inline Node* makeNum(double v) {
    Node* p = new Node();
    p->kind = 'N';
    p->num = v;
    return p;
}

// 创建变量节点
inline Node* makeVar(char c) {
    Node* p = new Node();
    p->kind = 'V';
    p->ch = c;
    return p;
}

// 创建运算符节点
inline Node* makeOp(char op, Node* L, Node* R) {
    Node* p = new Node();
    p->kind = 'O';
    p->ch = op;
    p->l = L;
    p->r = R;
    return p;
}

// 创建一元函数节点（sin/cos/tan/ln）
inline Node* makeFunc(const std::string& name, Node* child) {
    Node* p = new Node();
    p->kind = 'F';
    p->ch = funcCodeFromName(name);
    p->l = child;
    p->r = nullptr;
    return p;
}

// 判断是否为运算符
inline bool isOp(char c) {
    return c == '+' || c == '-' || c == '*' || c == '/' || c == '^';
}

// 判断是否为数字叶子节点
inline bool isNumLeaf(Node* p, double& v) {
    if (!p) return false;
    if (p->kind != 'N') return false;
    v = p->num;
    return true;
}

// 前向声明：表达式化简函数
inline Node* simplifyNode(Node* p);

// ===================== 表达式树类 =====================
struct ExprTree {
	Node* root = nullptr; // 根节点
	string postfixRaw;  // 后缀表达式字符串
    string infixCache;  // 缓存中缀表达式

    // 清空表达式树
    void clear() {
        freeTree(root);
        root = nullptr;
		postfixRaw.clear();  // 清空后缀表达式
        infixCache.clear();  // 清空中缀缓存
    }
    // 从树生成后缀表达式
    string toPostfix() const {
		string result;  // 后缀表达式结果
        std::function<void(Node*)> dfs = [&](Node* p) {
            if (!p) return;
            if (p->kind == 'N') {
                // 数字：如果是整数则不带小数点
                if (p->num == (int)p->num && p->num >= 0 && p->num <= 9) {
                    result += (char)('0' + (int)p->num);
                }
                else {
					std::ostringstream oss; // 多位数用括号标记
					oss << p->num;          // 转为字符串
                    result += "[" + oss.str() + "]";  // 用括号标记多位数
                }
                return;
            }
            if (p->kind == 'V') {
                result += p->ch;
                return;
            }
            if (p->kind == 'F') {
                dfs(p->l);
                result += funcNameFromCode(p->ch);  // 函数名
                return;
            }
            // 二元运算符：左 右 运算符
            dfs(p->l);
            dfs(p->r);
            result += p->ch;
            };
        dfs(root);
        return result;
    }

    // 更新后缀和中缀缓存
    void updateCaches() {
        postfixRaw = toPostfix();
        infixCache = toInfix();
    }
    // 从后缀表达式字符串构建表达式树
    bool buildFromPostfixChars(const string& s, string* err) {
        clear();
        postfixRaw = s;

		vector<Node*> st;   // 栈

        for (char c : s) {
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') continue;
            if (c >= '0' && c <= '9') {
                Node* p = new Node();
                p->kind = 'N';
                p->num = (double)(c - '0');
                st.push_back(p);
            }
            else if (c >= 'a' && c <= 'z') {
                Node* p = new Node();
                p->kind = 'V';
                p->ch = c;
                st.push_back(p);
            }
            else if (isOp(c)) {
                if ((int)st.size() < 2) {
                    if (err) *err = "操作数不足，表达式不合法";
                    for (auto* x : st) freeTree(x);
                    st.clear();
                    return false;
                }
                Node* b = st.back(); st.pop_back();
                Node* a = st.back(); st.pop_back();
                Node* p = new Node();
                p->kind = 'O';
                p->ch = c;
                p->l = a; p->r = b;
                st.push_back(p);
            }
            else {
                if (err) *err = string("非法字符：") + c;
                for (auto* x : st) freeTree(x);
                st.clear();
                return false;
            }
        }

        if (st.size() != 1) {
            if (err) *err = "表达式不合法：最终栈元素不为1";
            for (auto* x : st) freeTree(x);
            st.clear();
            return false;
        }
        root = st.back();
        infixCache = toInfix();  // 构建后自动缓存中缀表达式
        return true;
    }

    // 中缀输出（全括号，支持所有函数）
    string toInfix() const {
        std::function<string(Node*)> dfs = [&](Node* p)->string {
            if (!p) return "";
            if (p->kind == 'N') {
                std::ostringstream oss;
                oss << p->num;
                return oss.str();
            }
            if (p->kind == 'V') return string(1, p->ch);

            // 一元函数
            if (p->kind == 'F') {
                string fn = funcNameFromCode(p->ch);
                return fn + "(" + dfs(p->l) + ")";
            }

            // 二元运算
            string A = dfs(p->l);
            string B = dfs(p->r);
            return "(" + A + " " + string(1, p->ch) + " " + B + ")";
            };
        return dfs(root);
    }

    // 更新中缀缓存
    void updateInfixCache() {
        infixCache = toInfix();
    }

    // 收集表达式树中所有变量名
    std::set<char> collectVars() const {
        std::set<char> S;
        std::function<void(Node*)> dfs = [&](Node* p) {
            if (!p) return;
            if (p->kind == 'V') S.insert(p->ch);
            dfs(p->l); dfs(p->r);
            };
        dfs(root);
        return S;
    }

    // 计算表达式树的值（支持 sin/cos/tan/ln）
    bool eval(const std::map<char, double>& vars, double& out, string* err) const {
        std::function<bool(Node*, double&)> dfs = [&](Node* p, double& v)->bool {
            if (!p) { if (err) *err = "空节点"; return false; }

            if (p->kind == 'N') { v = p->num; return true; }

            if (p->kind == 'V') {
                auto it = vars.find(p->ch);
                if (it == vars.end()) {
                    if (err) *err = string("变量未赋值: ") + p->ch;
                    return false;
                }
                v = it->second;
                return true;
            }

            // 一元函数（支持 sin/cos/tan/ln）
            if (p->kind == 'F') {
                double x = 0;
                if (!dfs(p->l, x)) return false;

                switch (p->ch) {
                case 's': v = std::sin(x); return true;
                case 'c': v = std::cos(x); return true;
                case 't': v = std::tan(x); return true;
                case 'l':
                    if (x <= 0) {
                        if (err) *err = "ln 参数必须 > 0";
                        return false;
                    }
                    v = std::log(x);
                    return true;
                default:
                    if (err) *err = "未知函数节点";
                    return false;
                }
            }

            // 二元运算
            double x = 0, y = 0;
            if (!dfs(p->l, x)) return false;
            if (!dfs(p->r, y)) return false;

            switch (p->ch) {
            case '+': v = x + y; return true;
            case '-': v = x - y; return true;
            case '*': v = x * y; return true;
            case '/':
                if (std::fabs(y) < 1e-12) { if (err) *err = "除零错误"; return false; }
                v = x / y; return true;
            case '^':
                v = std::pow(x, y); return true;
            default:
                if (err) *err = string("未知运算符: ") + p->ch;
                return false;
            }
            };

        return dfs(root, out);
    }

    // 深拷贝整棵树
    ExprTree clone() const {
        ExprTree T;
        T.root = cloneTree(root);
        T.postfixRaw = postfixRaw;
        T.infixCache = infixCache;  // 同时拷贝中缀缓存
        return T;
    }

    // 表达式化简
    void simplify() {
        root = simplifyNode(root);
        updateCaches();  // 同时更新中缀和后缀
    }
};

// ===================== 复合表达式 =====================

// 构造复合表达式：(E1) op (E2)
inline ExprTree Compose(const ExprTree& E1, const ExprTree& E2, char op, string* err) {
    ExprTree R;
    if (!isOp(op)) { if (err) *err = "P 不是合法二元运算符"; return R; }
    if (!E1.root || !E2.root) { if (err) *err = "E1 或 E2 为空"; return R; }

    Node* p = new Node();
    p->kind = 'O'; p->ch = op;
    p->l = cloneTree(E1.root);
    p->r = cloneTree(E2.root);
    R.root = p;
    R.postfixRaw = E1.postfixRaw + E2.postfixRaw + op;
    return R;
}

// ===================== 偏导核心（支持三角函数/ln + 通用幂运算） =====================

// 对表达式树求偏导，返回新树节点（递归核心）
inline Node* derivNode(Node* p, char var, string* err) {
    if (!p) return nullptr;

    // 常量求导 = 0
    if (p->kind == 'N') return makeNum(0);

    // 变量求导：对自己=1，对其他=0
    if (p->kind == 'V') {
        return makeNum(p->ch == var ? 1 : 0);
    }

    // 一元函数：链式法则
    if (p->kind == 'F') {
        Node* u = p->l;
        Node* du = derivNode(u, var, err);  
        if (!du) return makeNum(0);

        // sin(u)' = cos(u) * u'
        if (p->ch == 's') {
            return makeOp('*', makeFunc("cos", cloneTree(u)), du);
        }

        // cos(u)' = -sin(u) * u'
        if (p->ch == 'c') {
            Node* negSin = makeOp('*', makeNum(-1), makeFunc("sin", cloneTree(u)));
            return makeOp('*', negSin, du);
        }

        // tan(u)' = (1 / cos(u)^2) * u'
        if (p->ch == 't') {
            Node* c   = makeFunc("cos", cloneTree(u));
            Node* c2  = makeOp('^', c, makeNum(2));
            Node* inv = makeOp('/', makeNum(1), c2);
            return makeOp('*', inv, du);
        }

        // ln(u)' = u'/u
        if (p->ch == 'l') {
            return makeOp('/', du, cloneTree(u));
        }

        if (err) *err = "不支持的函数求导";
        return makeNum(0);
    }

    // 运算符求导
    char op = p->ch;

    // (u + v)' = u' + v'  或  (u - v)' = u' - v'
    if (op == '+' || op == '-') {
        Node* dl = derivNode(p->l, var, err);
        Node* dr = derivNode(p->r, var, err);
        return makeOp(op, dl, dr);
    }

    // (u * v)' = u'*v + u*v'
    if (op == '*') {
        Node* du = derivNode(p->l, var, err);
        Node* dv = derivNode(p->r, var, err);
        Node* term1 = makeOp('*', du, cloneTree(p->r));
        Node* term2 = makeOp('*', cloneTree(p->l), dv);
        return makeOp('+', term1, term2);
    }

    // (u / v)' = (u'*v - u*v') / v^2
    if (op == '/') {
        Node* du = derivNode(p->l, var, err);
        Node* dv = derivNode(p->r, var, err);
        Node* nume1 = makeOp('*', du, cloneTree(p->r));
        Node* nume2 = makeOp('*', cloneTree(p->l), dv);
        Node* numerator = makeOp('-', nume1, nume2);
        Node* denom = makeOp('^', cloneTree(p->r), makeNum(2));
        return makeOp('/', numerator, denom);
    }

    // 通用幂运算求导：(u^v)' = u^v * (v' * ln(u) + v * u'/u)
    // 特例：v 是常数 => n * u^(n-1) * u'
    if (op == '^') {
        if (!p->l || !p->r) {
            if (err) *err = "乘幂节点缺少子表达式";
            return makeNum(0);
        }

        Node* u = p->l;
        Node* v = p->r;

        Node* du = derivNode(u, var, err);
        Node* dv = derivNode(v, var, err);
        if (!du || !dv) return makeNum(0);

        // 特例：v 是常数，用更简洁的 n*u^(n-1)*u'
        if (v->kind == 'N') {
            double n = v->num;

            // 释放不需要的 dv
            freeTree(dv);

            if (std::fabs(n) < 1e-12) {
                freeTree(du);
                return makeNum(0);   // u^0 = 1，导数为 0
            }
            if (std::fabs(n - 1.0) < 1e-12) {
                return du;           // u^1 = u，导数为 u'
            }

            Node* coef  = makeNum(n);
            Node* power = makeOp('^', cloneTree(u), makeNum(n - 1.0));
            return makeOp('*', makeOp('*', coef, power), du);
        }

        // 通用情况：u^v * ( v' * ln(u) + v * (u'/u) )
        Node* ln_u = makeFunc("ln", cloneTree(u));
        Node* term1 = makeOp('*', dv, ln_u);

        Node* u_div = makeOp('/', du, cloneTree(u));
        Node* term2 = makeOp('*', cloneTree(v), u_div);

        Node* inside = makeOp('+', term1, term2);
        Node* outer  = makeOp('^', cloneTree(u), cloneTree(v));

        return makeOp('*', outer, inside);
    }

    if (err) *err = "未知运算符，无法求导";
    return nullptr;
}

// 对表达式树求偏导，返回新的表达式树
inline ExprTree DerivativeTree(const ExprTree& T, char var, string* err) {
    ExprTree D;
    if (!T.root) { if (err) *err = "空表达式"; return D; }
    Node* r = derivNode(T.root, var, err);
    if (!r) return D;
    D.root = r;
	D.postfixRaw = "<derivative>";  // 标记为导数表达式
    return D;
}

// ===================== 树布局结构体 =====================

// 节点位置结构体
struct LayoutNodePos {
    int x;
    int y;
};

struct Layout {
    std::map<Node*, LayoutNodePos> pos;
};

// 递归计算树的布局（角度递减 + 线条较短）
inline Layout layoutTree(Node* root, int x, int y, int xGap, int yGap,
    int maxWidth = 700, int maxHeight = 350) {
    Layout L;
    if (!root) return L;

    // 计算树的深度
    std::function<int(Node*)> calcDepth = [&](Node* p) -> int {
        if (!p) return 0;
        return 1 + (std::max)(calcDepth(p->l), calcDepth(p->r));
        };

    int depth = calcDepth(root);
    if (depth == 0) return L;

    // 初始水平偏移量（较小值让线更短）
    const int baseOffset = 80;
    // 垂直间距（较小值让线更短）
    const int shortYGap = 55;
    // 衰减因子（0.65 让角度逐层缩小，避免重叠）
    const double decayFactor = 0.65;

    // 第一遍布局：角度逐层递减
    std::function<void(Node*, int, int, double)> layout = [&](Node* p, int cx, int cy, double offset) {
        if (!p) return;
        L.pos[p] = { cx, cy };

        // 计算下一层的偏移量
        double nextOffset = offset * decayFactor;
        if (nextOffset < 25) nextOffset = 25;  // 最小偏移量

        if (p->l) {
            layout(p->l, cx - (int)offset, cy + shortYGap, nextOffset);
        }
        if (p->r) {
            layout(p->r, cx + (int)offset, cy + shortYGap, nextOffset);
        }
        };

    layout(root, x, y, (double)baseOffset);

    // 第二遍：计算边界并检查是否需要缩放
    int minX = x, maxX = x, minY = y, maxY = y;
    for (auto& kv : L.pos) {
        if (kv.second.x < minX) minX = kv.second.x;
        if (kv.second.x > maxX) maxX = kv.second.x;
        if (kv.second.y < minY) minY = kv.second.y;
        if (kv.second.y > maxY) maxY = kv.second.y;
    }

    int currentWidth = maxX - minX;
    int currentHeight = maxY - minY;

    // 计算缩放比例（等比例缩放）
    double scaleX = (currentWidth > maxWidth) ? (double)maxWidth / currentWidth : 1.0;
    double scaleY = (currentHeight > maxHeight) ? (double)maxHeight / currentHeight : 1.0;
    double scale = (std::min)(scaleX, scaleY);

    // 如果需要缩放，重新调整所有节点位置
    if (scale < 1.0) {
        int centerX = (minX + maxX) / 2;
        int centerY = minY;

        for (auto& kv : L.pos) {
            int dx = kv.second.x - centerX;
            int dy = kv.second.y - centerY;
            kv.second.x = x + (int)(dx * scale);
            kv.second.y = y + (int)(dy * scale);
        }
    }

    return L;
}
// ===================== 辅助函数：提取项的系数和基底 =====================

// 从一个项中提取系数和基底
inline void extractCoefAndBase(Node* p, Node*& base, double& coef) {
    coef = 1.0;
    base = p;

    if (!p) return;

    // 如果是乘法节点
    if (p->kind == 'O' && p->ch == '*') {
        double lv = 0, rv = 0;
        bool lConst = isNumLeaf(p->l, lv);
        bool rConst = isNumLeaf(p->r, rv);

        // 形如 base * num
        if (rConst && !lConst) {
            coef = rv;
            base = p->l;
            return;
        }
        // 形如 num * base
        if (lConst && !rConst) {
            coef = lv;
            base = p->r;
            return;
        }
    }

    // 如果本身是数字
    if (p->kind == 'N') {
        coef = p->num;
        base = nullptr;  // 纯数字，没有基底
        return;
    }
}

// ===================== 辅助函数：判断两棵树是否结构相同 =====================

inline bool treesEqual(Node* a, Node* b) {
    if (!a && !b) return true;
    if (!a || !b) return false;
    if (a->kind != b->kind) return false;
    if (a->kind == 'N') return std::fabs(a->num - b->num) < 1e-12;
    if (a->kind == 'V') return a->ch == b->ch;
    if (a->kind == 'F') {
        if (a->ch != b->ch) return false;
        return treesEqual(a->l, b->l);
    }
    // 运算符节点
    if (a->ch != b->ch) return false;
    return treesEqual(a->l, b->l) && treesEqual(a->r, b->r);
}

// ===================== 辅助函数：收集加法链中的项 =====================

inline void collectAddTerms(Node* p, std::vector<Node*>& terms) {
    if (!p) return;
    if (p->kind == 'O' && p->ch == '+') {
        collectAddTerms(p->l, terms);
        collectAddTerms(p->r, terms);
    }
    else {
        terms.push_back(p);
    }
}

// ===================== 辅助函数：收集乘法链中的因子 =====================

inline void collectMulTerms(Node* p, std::vector<Node*>& terms) {
    if (!p) return;
    if (p->kind == 'O' && p->ch == '*') {
        collectMulTerms(p->l, terms);
        collectMulTerms(p->r, terms);
    }
    else {
        terms.push_back(p);
    }
}
// ===================== 替换变量为常量 =====================

// 将树中已赋值的变量替换为常量节点
inline Node* substituteVars(Node* p, const std::map<char, double>& varVals) {
    if (!p) return nullptr;

    // 如果是变量节点且已赋值，替换为数字节点
    if (p->kind == 'V') {
        auto it = varVals.find(p->ch);
        if (it != varVals.end()) {
            Node* numNode = makeNum(it->second);
            return numNode;
        }
        // 未赋值的变量，克隆保留
        return cloneTree(p);
    }

    // 数字节点直接克隆
    if (p->kind == 'N') {
        return cloneTree(p);
    }

    // 一元函数节点
    if (p->kind == 'F') {
        Node* newNode = new Node();
        newNode->kind = 'F';
        newNode->ch = p->ch;
        newNode->l = substituteVars(p->l, varVals);
        newNode->r = nullptr;
        return newNode;
    }

    // 二元运算符节点
    if (p->kind == 'O') {
        Node* newNode = new Node();
        newNode->kind = 'O';
        newNode->ch = p->ch;
        newNode->l = substituteVars(p->l, varVals);
        newNode->r = substituteVars(p->r, varVals);
        return newNode;
    }

    return cloneTree(p);
}
// ===================== 表达式化简（支持同类项合并 + 系数合并） =====================

inline Node* simplifyNode(Node* p) {
    if (!p) return nullptr;

    p->l = simplifyNode(p->l);
    p->r = simplifyNode(p->r);

    // 一元函数：如果参数是常量，直接计算
    if (p->kind == 'F') {
        double v = 0;
        if (isNumLeaf(p->l, v)) {
            double result = 0;
            bool valid = true;
            switch (p->ch) {
            case 's': result = std::sin(v); break;
            case 'c': result = std::cos(v); break;
            case 't': result = std::tan(v); break;
            case 'l':
                if (v > 0) result = std::log(v);
                else valid = false;
                break;
            default: return p;
            }
            if (valid) {
                freeTree(p->l);
                p->l = nullptr;
                delete p;
                return makeNum(result);
            }
        }
        return p;
    }

    // 二元运算符化简
    if (p->kind == 'O') {
        double lv = 0, rv = 0;
        bool lConst = isNumLeaf(p->l, lv);
        bool rConst = isNumLeaf(p->r, rv);

        // 两边都是常量：直接计算
        if (lConst && rConst) {
            double result = 0;
            bool valid = true;
            switch (p->ch) {
            case '+': result = lv + rv; break;
            case '-': result = lv - rv; break;
            case '*': result = lv * rv; break;
            case '/':
                if (std::fabs(rv) < 1e-12) valid = false;
                else result = lv / rv;
                break;
            case '^': result = std::pow(lv, rv); break;
            default: valid = false;
            }
            if (valid) {
                freeTree(p->l); freeTree(p->r);
                p->l = p->r = nullptr;
                delete p;
                return makeNum(result);
            }
        }

        char op = p->ch;

        // 同类项合并（支持系数）：a + a + a → a * 3，a*4 + a*5 → a * 9
        if (op == '+') {
            std::vector<Node*> terms;
            collectAddTerms(p, terms);

            if (terms.size() >= 2) {
                // 提取每项的系数和基底
                struct TermInfo {
                    Node* base;      // 基底（可能为 nullptr 表示纯数字）
                    double coef;     // 系数
                };
                std::vector<TermInfo> infos;
                for (Node* t : terms) {
                    TermInfo ti;
                    extractCoefAndBase(t, ti.base, ti.coef);
                    infos.push_back(ti);
                }

                // 按基底分组合并系数
                std::vector<std::pair<Node*, double>> grouped;  // base -> total coef
                std::vector<bool> used(infos.size(), false);

                for (int i = 0; i < (int)infos.size(); ++i) {
                    if (used[i]) continue;
                    double totalCoef = infos[i].coef;
                    Node* base = infos[i].base;

                    for (int j = i + 1; j < (int)infos.size(); ++j) {
                        if (used[j]) continue;
                        bool sameBase = (base == nullptr && infos[j].base == nullptr) ||
                            (base != nullptr && infos[j].base != nullptr && treesEqual(base, infos[j].base));
                        if (sameBase) {
                            totalCoef += infos[j].coef;
                            used[j] = true;
                        }
                    }
                    grouped.push_back({ base, totalCoef });
                    used[i] = true;
                }

                // 如果有合并发生
                if (grouped.size() < terms.size()) {
                    // 先构建新树（在释放原树之前克隆需要的节点）
                    Node* result = nullptr;
                    for (auto& g : grouped) {
                        Node* term;
                        if (g.first == nullptr) {
                            term = makeNum(g.second);
                        }
                        else if (std::fabs(g.second - 1.0) < 1e-12) {
                            term = cloneTree(g.first);  // 必须克隆
                        }
                        else if (std::fabs(g.second) < 1e-12) {
                            continue;
                        }
                        else {
                            term = makeOp('*', cloneTree(g.first), makeNum(g.second));  
                        }
                        if (!result) {
                            result = term;
                        }
                        else {
                            result = makeOp('+', result, term);
                        }
                    }
                    if (!result) result = makeNum(0);

                    // 安全释放原树
                    freeTree(p);
                    return simplifyNode(result);
                }
            }
        }

        // 同类因子合并：a * a * a → a ^ 3
        if (op == '*') {
            std::vector<Node*> factors;
            collectMulTerms(p, factors);

            if (factors.size() >= 2) {
                double numProduct = 1.0;
                std::vector<Node*> nonNumFactors;

                for (Node* f : factors) {
                    double v = 0;
                    if (isNumLeaf(f, v)) {
                        numProduct *= v;
                    }
                    else {
                        nonNumFactors.push_back(f);
                    }
                }

                std::vector<std::pair<Node*, int>> grouped;
                std::vector<bool> used(nonNumFactors.size(), false);

                for (int i = 0; i < (int)nonNumFactors.size(); ++i) {
                    if (used[i]) continue;
                    int count = 1;
                    for (int j = i + 1; j < (int)nonNumFactors.size(); ++j) {
                        if (!used[j] && treesEqual(nonNumFactors[i], nonNumFactors[j])) {
                            count++;
                            used[j] = true;
                        }
                    }
                    grouped.push_back({ nonNumFactors[i], count });
                    used[i] = true;
                }

                // 检查是否有任何合并
                size_t expectedFactors = nonNumFactors.size() + (std::fabs(numProduct - 1.0) >= 1e-12 ? 1 : 0);
                bool hasNumMerge = (factors.size() > expectedFactors);
                bool hasVarMerge = (grouped.size() < nonNumFactors.size());

                if (hasNumMerge || hasVarMerge) {
                    if (std::fabs(numProduct) < 1e-12) {
                        freeTree(p);
                        return makeNum(0);
                    }

                    // 先构建新树
                    Node* result = nullptr;

                    if (std::fabs(numProduct - 1.0) >= 1e-12) {
                        result = makeNum(numProduct);
                    }

                    for (auto& g : grouped) {
                        Node* term;
                        if (g.second == 1) {
                            term = cloneTree(g.first);  // 必须克隆
                        }
                        else {
                            term = makeOp('^', cloneTree(g.first), makeNum((double)g.second));  // 必须克隆
                        }
                        if (!result) {
                            result = term;
                        }
                        else {
                            result = makeOp('*', result, term);
                        }
                    }

                    if (!result) result = makeNum(numProduct);

                    //安全释放原树
                    freeTree(p);
                    return simplifyNode(result);
                }
            }
        }

        // x + 0 = x, 0 + x = x
        if (op == '+') {
            if (rConst && std::fabs(rv) < 1e-12) {
                Node* ret = p->l; p->l = nullptr;
                freeTree(p->r); delete p;
                return ret;
            }
            if (lConst && std::fabs(lv) < 1e-12) {
                Node* ret = p->r; p->r = nullptr;
                freeTree(p->l); delete p;
                return ret;
            }
        }

        // x - 0 = x
        if (op == '-' && rConst && std::fabs(rv) < 1e-12) {
            Node* ret = p->l; p->l = nullptr;
            freeTree(p->r); delete p;
            return ret;
        }

        // x * 0 = 0, 0 * x = 0, x * 1 = x, 1 * x = x
        if (op == '*') {
            if ((rConst && std::fabs(rv) < 1e-12) || (lConst && std::fabs(lv) < 1e-12)) {
                freeTree(p->l); freeTree(p->r);
                p->l = p->r = nullptr;
                delete p;
                return makeNum(0);
            }
            if (rConst && std::fabs(rv - 1) < 1e-12) {
                Node* ret = p->l; p->l = nullptr;
                freeTree(p->r); delete p;
                return ret;
            }
            if (lConst && std::fabs(lv - 1) < 1e-12) {
                Node* ret = p->r; p->r = nullptr;
                freeTree(p->l); delete p;
                return ret;
            }
        }

        // x / 1 = x
        if (op == '/' && rConst && std::fabs(rv - 1) < 1e-12) {
            Node* ret = p->l; p->l = nullptr;
            freeTree(p->r); delete p;
            return ret;
        }

        // x ^ 0 = 1, x ^ 1 = x
        if (op == '^') {
            if (rConst && std::fabs(rv) < 1e-12) {
                freeTree(p->l); freeTree(p->r);
                p->l = p->r = nullptr;
                delete p;
                return makeNum(1);
            }
            if (rConst && std::fabs(rv - 1) < 1e-12) {
                Node* ret = p->l; p->l = nullptr;
                freeTree(p->r); delete p;
                return ret;
            }
        }
    }

    return p;
}

#endif // PPE_H
