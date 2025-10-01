// file: hello.c
int v[5]={0x2227, 0x32, 0x33, 0x34, 0x35};
int add(int a, int b) {
    return a + b;
}
int sub(int a, int b) {
  int c = a - b;
  v[a] = c;
  int d = c + v[0]+v[a];
  return d;
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
