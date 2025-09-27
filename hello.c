// file: hello.c
int add(int a, int b) {
    return a + b;
}
int sub(int a, int b) {
  return a - b
    + *(int*)1
    ;
}
int v=0x33;  
int sub2(int a, int b) {
  v = a * b;
  return v + a;
}
void hiya(int* a, float b){
  *a = (int)b;
}
int getit(void){
  return 48;
}
void donot(void){
}
