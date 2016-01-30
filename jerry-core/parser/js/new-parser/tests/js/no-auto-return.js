/* This test contains no auto return parser tests. */

function f()
{
  a + b
}

function g()
{
  return 0 + y * 128;
}

function h()
{
  if (x) return 127 + -127 + -+-127;
}

function i()
{
  while (true) { return }
}

function k()
{
  if (x) return 1
  else return 2
}

