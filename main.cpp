#include <graphics.h>
#include <conio.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <vector>
#include <queue>
#include <stack>
#include <string>
#include <sstream>
#include <iostream>
#include <limits>
#include <windows.h>

using namespace std;

/* --- Window / layout constants --- */
const int WIN_W = 1000;
const int WIN_H = 650;
const int UI_H  = 70;
const int NODE_RADIUS = 22;
const unsigned long DOUBLE_CLICK_MS = 400UL;

/* --- Basic node type --- */
struct Node {
    int x, y;
    string label;
    bool visited;
};

/* --- Interaction modes --- */
enum Mode {
    MODE_ADD_NODE,
    MODE_ADD_EDGE,
    MODE_BFS,
    MODE_DFS,
    MODE_SELF_LOOP,
    MODE_DELETE_NODE,
    MODE_DELETE_EDGE,
    MODE_NONE
};

/* --- Global state --- */
static vector<Node> nodes;
static vector< vector< pair<int,int> > > adj; // adjacency: (to, weight)
static int nodeCount = 0;
static Mode currentMode = MODE_ADD_NODE;
static int selNode = -1;

/* --- Undo/Redo snapshot --- */
struct Snapshot {
    vector<Node> s_nodes;
    vector< vector< pair<int,int> > > s_adj;
    int s_nodeCount;
};
static vector<Snapshot> undoStack;
static vector<Snapshot> redoStack;
const int MAX_UNDO = 120;

/* --- Double buffering pages --- */
static int activePage = 0;
static int visualPage = 1;

/* --- Graph options --- */
static bool GLOBAL_WEIGHTED = false;
static bool GLOBAL_DIRECTED = false;

/* --- Double-click tracking --- */
static unsigned long lastClickTime = 0;
static int lastClickNode = -1;

/* --- Utility: int -> string --- */
string intToStr(int v) { stringstream ss; ss << v; return ss.str(); }

/* --- Save current state for undo --- */
void saveSnapshot() {
    Snapshot s;
    s.s_nodes = nodes;
    s.s_adj = adj;
    s.s_nodeCount = nodeCount;
    undoStack.push_back(s);
    if ((int)undoStack.size() > MAX_UNDO) undoStack.erase(undoStack.begin());
    redoStack.clear();
}

/* --- Apply a snapshot (undo/redo) --- */
void applySnapshot(const Snapshot &s) {
    nodes = s.s_nodes;
    adj = s.s_adj;
    nodeCount = s.s_nodeCount;
}

/* --- Find node under a point (returns index or -1) --- */
int findNodeAt(int mx, int my) {
    for (int i = 0; i < nodeCount; ++i) {
        int dx = mx - nodes[i].x;
        int dy = my - nodes[i].y;
        if (dx*dx + dy*dy <= NODE_RADIUS*NODE_RADIUS) return i;
    }
    return -1;
}

/* --- Simple rect hit test --- */
bool pointInRect(int mx,int my,int x,int y,int w,int h) {
    return mx >= x && mx <= x + w && my >= y && my <= y + h;
}

/* --- Draw a toolbar button --- */
void drawButton(int x,int y,int w,int h,const string &txt,bool active,bool hover) {
    int fill = active ? LIGHTCYAN : (hover ? LIGHTGRAY+2 : LIGHTGRAY);
    setfillstyle(SOLID_FILL, fill);
    bar(x,y,x+w,y+h);
    setcolor(BLACK);
    rectangle(x,y,x+w,y+h);
    setbkcolor(fill);
    outtextxy(x+10, y + (h/2 - textheight((char*)txt.c_str())/2), (char*)txt.c_str());
}

/* --- Draw the UI toolbar --- */
void drawUI(int hoverButtonIndex) {
    setfillstyle(SOLID_FILL, LIGHTGRAY);
    bar(0, 0, WIN_W, UI_H);
    rectangle(0,0,WIN_W-1,UI_H-1);

    drawButton(10,15,110,40,"Add Node", currentMode==MODE_ADD_NODE, hoverButtonIndex==0);
    drawButton(130,15,110,40,"Add Edge", currentMode==MODE_ADD_EDGE, hoverButtonIndex==1);
    drawButton(250,15,80,40,"BFS", currentMode==MODE_BFS, hoverButtonIndex==2);
    drawButton(340,15,80,40,"DFS", currentMode==MODE_DFS, hoverButtonIndex==3);
    drawButton(430,15,80,40,"Undo", false, hoverButtonIndex==4);
    drawButton(520,15,80,40,"Redo", false, hoverButtonIndex==5);
    drawButton(610,15,100,40,"Self Loop", currentMode==MODE_SELF_LOOP, hoverButtonIndex==6);
    drawButton(730,15,80,40,"Clear", false, hoverButtonIndex==7);

    setcolor(BLACK);
    string s1 = "Weighted: "; s1 += GLOBAL_WEIGHTED ? "YES" : "NO";
    string s2 = "Directed: "; s2 += GLOBAL_DIRECTED ? "YES" : "NO";
    outtextxy(10,3,(char*)s1.c_str());
    outtextxy(200,3,(char*)s2.c_str());
}

/* --- Draw an arrow head between two points --- */
void drawArrowHead(int x1,int y1,int x2,int y2) {
    double dx = x2 - x1, dy = y2 - y1;
    double len = sqrt(dx*dx + dy*dy);
    if (len < 1.0) return;
    double ux = dx/len, uy = dy/len;
    int back = 12, side = 6;
    double bx = x2 - ux*back, by = y2 - uy*back;
    double sx = -uy*side, sy = ux*side;
    int ax = (int)(bx + sx), ay = (int)(by + sy);
    int bx2 = (int)(bx - sx), by2 = (int)(by - sy);
    line(x2,y2,ax,ay); line(ax,ay,bx2,by2); line(bx2,by2,x2,y2);
}

/* --- Draw a self-loop clearly outside the node (always visible) --- */
void drawSelfLoop(int idx) {
    int x = nodes[idx].x;
    int y = nodes[idx].y;
    int r = NODE_RADIUS;
    int ovalW = r + 10;
    int ovalH = r/2 + 6;
    int cx = x + r + 8;
    int cy = y - r - 8;

    setcolor(DARKGRAY);
    setlinestyle(SOLID_LINE, 0, 2);
    ellipse(cx, cy, 0, 360, ovalW, ovalH);

    int ax = cx - ovalW/2 + 2;
    int ay = cy + ovalH/2 - 2;
    drawArrowHead(ax-4, ay-3, ax, ay);

    if(GLOBAL_WEIGHTED){
        int w = -1;
        for(int k=0;k<(int)adj[idx].size();k++){
            if(adj[idx][k].first==idx){ w = adj[idx][k].second; break; }
        }
        if(w>=0){
            string ws = intToStr(w);
            int tw = textwidth((char*)ws.c_str()), th = textheight((char*)ws.c_str());
            int tx = cx - tw/2;
            int ty = cy - ovalH - th - 4;
            setfillstyle(SOLID_FILL, LIGHTGRAY);
            bar(tx-4, ty-2, tx+tw+4, ty+th+2);
            outtextxy(tx, ty, (char*)ws.c_str());
        }
    }
}

/* --- Draw an edge between two nodes --- */
void drawEdgeVisual(int a,int b,int weight) {
    int x1 = nodes[a].x, y1 = nodes[a].y;
    int x2 = nodes[b].x, y2 = nodes[b].y;
    double dx = x2 - x1, dy = y2 - y1;
    double dist = sqrt(dx*dx + dy*dy);
    if(dist<1.0) return;
    double ux = dx/dist, uy = dy/dist;
    int sx = (int)(x1 + ux*NODE_RADIUS), sy = (int)(y1 + uy*NODE_RADIUS);
    int ex = (int)(x2 - ux*NODE_RADIUS), ey = (int)(y2 - uy*NODE_RADIUS);
    setcolor(DARKGRAY);
    line(sx,sy,ex,ey);
    if(GLOBAL_WEIGHTED){
        string ws = intToStr(weight);
        int mx = (sx+ex)/2, my=(sy+ey)/2;
        int tw=textwidth((char*)ws.c_str()), th=textheight((char*)ws.c_str());
        setfillstyle(SOLID_FILL,LIGHTGRAY);
        bar(mx-tw/2-4,my-th/2-2,mx+tw/2+4,my+th/2+2);
        outtextxy(mx-tw/2,my-th/2,(char*)ws.c_str());
    }
    if(GLOBAL_DIRECTED) drawArrowHead(sx,sy,ex,ey);
}

/* --- Main redraw: edges, nodes, self-loops (self-loops on top) --- */
void drawAll(int hoverNode=-1,int hoverButton=-1){
    setactivepage(activePage);
    setfillstyle(SOLID_FILL, WHITE);
    bar(0,UI_H,WIN_W,WIN_H);
    drawUI(hoverButton);

    for(int i=0;i<(int)adj.size();i++){
        for(int j=0;j<(int)adj[i].size();j++){
            int to = adj[i][j].first;
            if(to != i){
                if(GLOBAL_DIRECTED || to > i) drawEdgeVisual(i,to,adj[i][j].second);
            }
        }
    }

    for(int i=0;i<nodeCount;i++){
        int fill=LIGHTCYAN;
        if(nodes[i].visited) fill=LIGHTGREEN;
        if(i==hoverNode) fill=YELLOW;
        setfillstyle(SOLID_FILL, fill);
        fillellipse(nodes[i].x,nodes[i].y,NODE_RADIUS,NODE_RADIUS);
        setcolor(BLACK);
        circle(nodes[i].x,nodes[i].y,NODE_RADIUS);
        string lab=nodes[i].label;
        int tw=textwidth((char*)lab.c_str()), th=textheight((char*)lab.c_str());
        outtextxy(nodes[i].x-tw/2,nodes[i].y-th/2,(char*)lab.c_str());
    }

    for(int i=0;i<(int)adj.size();i++){
        for(int j=0;j<(int)adj[i].size();j++){
            int to = adj[i][j].first;
            if(to == i) drawSelfLoop(i);
        }
    }

    setvisualpage(activePage);
    activePage = 1-activePage;
    visualPage = 1-visualPage;
}

/* --- BFS / DFS visualization helpers --- */
void resetVisited(){ for(int i=0;i<nodeCount;i++) nodes[i].visited=false; }

void visualizeVisit(int idx,int delayMs){
    nodes[idx].visited=true;
    drawAll();
    delay(delayMs);
}

void BFS_visual(int start){
    if(start<0||start>=nodeCount) return;
    resetVisited();
    queue<int> q; q.push(start); nodes[start].visited=true;
    while(!q.empty()){
        int u=q.front(); q.pop();
        visualizeVisit(u,220);
        for(int k=0;k<(int)adj[u].size();k++){
            int v=adj[u][k].first;
            if(!nodes[v].visited){ nodes[v].visited=true; q.push(v);}
        }
    }
}

void DFS_visual(int start){
    if(start<0||start>=nodeCount) return;
    resetVisited();
    stack<int> st; st.push(start);
    while(!st.empty()){
        int u=st.top(); st.pop();
        if(nodes[u].visited) continue;
        visualizeVisit(u,220);
        for(int i=(int)adj[u].size()-1;i>=0;i--){
            int v=adj[u][i].first;
            if(!nodes[v].visited) st.push(v);
        }
    }
}

/* --- Geometry helper: distance point-to-segment --- */
double distPointToSegment(int px,int py,int ax,int ay,int bx,int by){
    double vx=bx-ax,vy=by-ay;
    double wx=px-ax,wy=py-ay;
    double c1=vx*wx+vy*wy;
    double c2=vx*vx+vy*vy;
    double t=(c2<1e-6)?0.0:c1/c2;
    if(t<0)t=0;if(t>1)t=1;
    double cx=ax+t*vx,cy=ay+t*vy;
    double dx=px-cx,dy=py-cy;
    return sqrt(dx*dx+dy*dy);
}

/* --- Find an edge near a point (for delete) --- */
pair<int,int> findEdgeNear(int mx,int my,double threshold=8.0){
    for(int u=0;u<(int)adj.size();u++){
        for(int k=0;k<(int)adj[u].size();k++){
            int v=adj[u][k].first;
            if(u==v) continue;
            if(!GLOBAL_DIRECTED && v<u) continue;
            int ax=nodes[u].x, ay=nodes[u].y, bx=nodes[v].x, by=nodes[v].y;
            double d=distPointToSegment(mx,my,ax,ay,bx,by);
            if(d<=threshold) return make_pair(u,k);
        }
    }
    // Self-loop proximity
    for(int u=0;u<(int)adj.size();u++){
        for(int k=0;k<(int)adj[u].size();k++){
            if(adj[u][k].first==u){
                int cx=nodes[u].x;
                int cy=nodes[u].y-NODE_RADIUS-(NODE_RADIUS/2);
                int dx=mx-cx, dy=my-cy;
                if(dx*dx+dy*dy<=(NODE_RADIUS/2+8)*(NODE_RADIUS/2+8)) return make_pair(u,k);
            }
        }
    }
    return make_pair(-1,-1);
}

/* --- Undo / redo implementation --- */
void doUndo(){
    if((int)undoStack.size()==0) return;
    Snapshot cur; cur.s_nodes=nodes; cur.s_adj=adj; cur.s_nodeCount=nodeCount;
    redoStack.push_back(cur);
    Snapshot s=undoStack.back(); undoStack.pop_back();
    applySnapshot(s); drawAll();
}

void doRedo(){
    if((int)redoStack.size()==0) return;
    Snapshot cur; cur.s_nodes=nodes; cur.s_adj=adj; cur.s_nodeCount=nodeCount;
    undoStack.push_back(cur);
    Snapshot s=redoStack.back(); redoStack.pop_back();
    applySnapshot(s); drawAll();
}

/* --- Popup: ask the user for edge weight
     (Centered text in the input box; popup clamped to window.) --- */
int popupGetWeight(int px,int py){
    int boxW=260, boxH=100;
    int bx = px - boxW/2;
    int by = py - boxH/2;

    if(bx < 10) bx = 10;
    if(bx + boxW > WIN_W - 10) bx = WIN_W - boxW - 10;
    if(by + boxH > WIN_H - 20) by = WIN_H - boxH - 20;
    if(by < UI_H + 20) by = UI_H + 20;

    string text = "";
    bool done=false, canceled=false;
    settextstyle(3,0,2);
    setbkcolor(LIGHTGRAY);

    setactivepage(activePage);
    setvisualpage(activePage);

    while(!done){
        setactivepage(activePage);

        // draw popup background
        setfillstyle(SOLID_FILL,LIGHTGRAY);
        bar(bx,by,bx+boxW,by+boxH);
        setcolor(BLACK);
        rectangle(bx,by,bx+boxW,by+boxH);

        // title
        outtextxy(bx+10, by+10, (char*)"Enter Edge Weight (digits)");

        // input field rectangle
        int inputY = by + 40, inputH = 30;
        rectangle(bx+10, inputY, bx+boxW-10, inputY+inputH);

        // centered display text inside the input rectangle (horizontally + vertically)
        string display = text.empty() ? "" : text;
        int tw = textwidth((char*)display.c_str());
        int th = textheight((char*)display.c_str());
        int tx = bx + (boxW - tw) / 2;                     // center horizontally
        int ty = inputY + (inputH - th) / 2;               // center vertically
        outtextxy(tx, ty, (char*)display.c_str());

        setvisualpage(activePage);

        delay(20);
        if(kbhit()){
            int c = getch();
            if(c==13){ if(!text.empty()) done=true; }
            else if(c==27){ canceled=true; done=true; }
            else if(c==8){ if(!text.empty()) text.erase(text.size()-1); }
            else if(c>='0' && c<='9' && text.size()<6) text.push_back((char)c);
        }
    }

    settextstyle(DEFAULT_FONT,0,1);
    if(canceled) return -1;
    int val=1; stringstream ss(text); ss>>val; if(val<=0) val=1;
    return val;
}

/* --- Program entry: initialize, main loop, input handling --- */
int main(){
    int wchoice=0, dchoice=0;
    while(true){ cout<<"Weighted graph? (1=Yes,0=No): "; if(cin>>wchoice && (wchoice==0||wchoice==1)) break; cin.clear(); cin.ignore(numeric_limits<streamsize>::max(),'\n'); }
    while(true){ cout<<"Directed graph? (1=Yes,0=No): "; if(cin>>dchoice && (dchoice==0||dchoice==1)) break; cin.clear(); cin.ignore(numeric_limits<streamsize>::max(),'\n'); }
    GLOBAL_WEIGHTED = (wchoice==1);
    GLOBAL_DIRECTED = (dchoice==1);

    initwindow(WIN_W, WIN_H, "Graph Editor - Dev-C++ / WinBGIm");
    activePage=0; visualPage=1; setactivepage(activePage); setvisualpage(visualPage); cleardevice();

    nodes.clear(); adj.clear(); nodeCount=0; currentMode=MODE_ADD_NODE; selNode=-1;
    undoStack.clear(); redoStack.clear();
    Snapshot base; base.s_nodes=nodes; base.s_adj=adj; base.s_nodeCount=nodeCount;
    undoStack.push_back(base);

    drawAll();

    int prevHoverNode=-1, prevHoverButton=-1;
    bool running=true;
    while(running){
        if(kbhit()){
            char ch=getch();
            if(ch==27) break;
            if(ch=='u'||ch=='U'){ doUndo(); continue; }
            if(ch=='r'||ch=='R'){ doRedo(); continue; }
        }

        int mx = mousex(), my = mousey();
        int hoverNode = findNodeAt(mx,my);
        int hoverButton = -1;

        if(pointInRect(mx,my,10,15,110,40)) hoverButton=0;
        else if(pointInRect(mx,my,130,15,110,40)) hoverButton=1;
        else if(pointInRect(mx,my,250,15,80,40)) hoverButton=2;
        else if(pointInRect(mx,my,340,15,80,40)) hoverButton=3;
        else if(pointInRect(mx,my,430,15,80,40)) hoverButton=4;
        else if(pointInRect(mx,my,520,15,80,40)) hoverButton=5;
        else if(pointInRect(mx,my,610,15,100,40)) hoverButton=6;
        else if(pointInRect(mx,my,730,15,80,40)) hoverButton=7;

        if(prevHoverNode!=hoverNode || prevHoverButton!=hoverButton){
            drawAll(hoverNode, hoverButton);
            prevHoverNode = hoverNode; prevHoverButton = hoverButton;
        }

        if(ismouseclick(WM_LBUTTONDOWN)){
            clearmouseclick(WM_LBUTTONDOWN);

            if(hoverButton!=-1){
                switch(hoverButton){
                    case 0: currentMode=MODE_ADD_NODE; selNode=-1; break;
                    case 1: currentMode=MODE_ADD_EDGE; selNode=-1; break;
                    case 2: currentMode=MODE_BFS; selNode=-1; break;
                    case 3: currentMode=MODE_DFS; selNode=-1; break;
                    case 4: doUndo(); break;
                    case 5: doRedo(); break;
                    case 6: currentMode=MODE_SELF_LOOP; selNode=-1; break;
                    case 7: saveSnapshot(); nodes.clear(); adj.clear(); nodeCount=0; selNode=-1; drawAll(); break;
                }
                continue;
            }

            if(currentMode==MODE_ADD_NODE && my>UI_H+10){
                saveSnapshot();
                Node n; n.x=mx; n.y=my; n.label=intToStr(nodeCount); n.visited=false;
                nodes.push_back(n);
                adj.resize(nodes.size());
                nodeCount++;
                drawAll();
            }
            else if(currentMode==MODE_ADD_EDGE){
                int id = findNodeAt(mx,my);
                if(id!=-1){
                    unsigned long now = GetTickCount();
                    if(lastClickNode==id && now-lastClickTime<=DOUBLE_CLICK_MS){
                        lastClickNode = -1;
                        lastClickTime = 0;
                        selNode = -1;
                        continue;
                    }

                    if(selNode==-1){ selNode=id; lastClickNode=id; lastClickTime=now; }
                    else if(selNode!=id){
                        saveSnapshot();
                        int w=1;
                        if(GLOBAL_WEIGHTED){
                            int got = popupGetWeight((nodes[selNode].x+nodes[id].x)/2, (nodes[selNode].y+nodes[id].y)/2);
                            if(got<0){ selNode=-1; drawAll(); continue; }
                            w = got;
                        }
                        adj[selNode].push_back(make_pair(id,w));
                        if(!GLOBAL_DIRECTED) adj[id].push_back(make_pair(selNode,w));
                        selNode=-1; drawAll();
                    }
                }
            }
            else if(currentMode==MODE_SELF_LOOP){
                int id = findNodeAt(mx,my);
                if(id!=-1){
                    bool exists=false;
                    for(int k=0;k<(int)adj[id].size();k++) if(adj[id][k].first==id) exists=true;
                    if(!exists){
                        saveSnapshot();
                        int w = 1;
                        if(GLOBAL_WEIGHTED){
                            int got = popupGetWeight(nodes[id].x, nodes[id].y - NODE_RADIUS - 10);
                            if(got<0){ drawAll(); continue; }
                            w = got;
                        }
                        adj[id].push_back(make_pair(id,w));
                        drawAll();
                    }
                }
            }
            else if(currentMode==MODE_DELETE_NODE){
                int id=findNodeAt(mx,my);
                if(id!=-1){
                    saveSnapshot();
                    nodes.erase(nodes.begin()+id);
                    adj.erase(adj.begin()+id);
                    for(int i=0;i<(int)adj.size();i++){
                        for(int j=0;j<(int)adj[i].size();j++){
                            if(adj[i][j].first==id){ adj[i][j].first = -1; }
                        }
                        for(int j=0;j<(int)adj[i].size();){
                            if(adj[i][j].first==-1) { adj[i].erase(adj[i].begin()+j); }
                            else {
                                if(adj[i][j].first>id) adj[i][j].first--;
                                j++;
                            }
                        }
                    }
                    nodeCount--; drawAll();
                }
            }
            else if(currentMode==MODE_DELETE_EDGE){
                pair<int,int> e=findEdgeNear(mx,my);
                if(e.first!=-1){
                    saveSnapshot();
                    adj[e.first].erase(adj[e.first].begin()+e.second);
                    drawAll();
                }
            }
            else if(currentMode==MODE_BFS){
                int id=findNodeAt(mx,my);
                if(id!=-1) BFS_visual(id);
            }
            else if(currentMode==MODE_DFS){
                int id=findNodeAt(mx,my);
                if(id!=-1) DFS_visual(id);
            }
        }
    }

    closegraph();
    return 0;
}

