/* This test contains object literal parser tests. */

a = {
  get x() {},
  set x(value) {},
  y: 6,
  get z() {},
  set z(value) {}
};

b = {
  v:1,
  get f() {},
  set f(value) {},
  get f() {},
};

