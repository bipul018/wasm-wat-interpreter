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

//int sub2(int a, int b) {
//  v = a * b;
//  return v + a;
//}
//void hiya(int* a, float b){
//  *a = (int)b;
//}
//int getit(void){
//  return 48;
//}
//void donot(void){
//}
