static int param = 123;

int sum(int a, int b) {
  return a + b;
}

int main()
{
  int res = sum(1, param);
  return res;
}
