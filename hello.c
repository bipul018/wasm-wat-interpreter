// file: hello.c
int v[5]={0x2227, 0x32, 0x33, 0x34, 0x35};
int add(int a, int b) {
    return a + b;
}
int sub(int a, int b) {
  int c = a - b;
  v[a] = c;
  int d = c -a*b + v[0]+v[a];
  return d;
}
int abs_diff(int x, int y){
  if(x > y) {
    return x - y;
  } else {
    return y - x;
  }
}
int pick_branch(int x, int y) {
  if (x > 0) {
    if (y > 0) {
      return x-y;
    } else {
      return x*y;
    }
  } else {
    return x*x+y*2;
  }
}
int fibo(int n){
  int a0 = 0;
  int a1 = 1;

  while(n > 0){
    n--;
    int c = a0 + a1;
    a0 = a1;
    a1 = c;
  }
  return a0;
}
int fibo_rec(int n){
  if(n <= 1) return n;
  return fibo_rec(n-1) + fibo_rec(n-2);
}
extern void printint(int x);
extern void printhex(int x);
void print_sum(int a, int b){
  printint(a+b);
}

void init_window(int w, int h);
void begin_drawing(void);
void end_drawing(void);
int window_should_close(void);
void clear_background(unsigned);
void draw_fps(int x, int y);

void printstr(const char* fmtstr, ...);

void run_raylib(int dummy_arg){
  (void)dummy_arg;
  init_window(800, 800);

  unsigned cols[] = {
    0xffffffff,
    0xffffff00,
    0xffff00ff,
    0xff00ffff,
    0xffff0000,
    0xff00ff00,
    0xff0000ff,
    0xff000000,
  };

  const int count = sizeof(cols)/sizeof(cols[0]);

  int inx = 0;
  const int steps = 100;

  printstr("Hello, beginning the %s operation now %d %d\n", "<printing>", 0xf3, 0x4a);

  while(window_should_close() == 0){
    begin_drawing();
    const int i = (inx / steps) % count;
    //printint(i);
    //printhex(cols[i]);
    clear_background(cols[i]);

    draw_fps(10, 10);

    end_drawing();
    inx++;
  }
  printhex(0xff00ff00);
}
