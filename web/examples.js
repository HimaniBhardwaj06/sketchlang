// Example programs shown in the playground's dropdown.
const SK_EXAMPLES = [
  {
    name: "Row of circles",
    code: `// draw a row of circles
let x = 10
repeat 4 {
  circle(x, 40, 8)
  x = x + 20
}
`,
  },
  {
    name: "Rings",
    code: `// concentric rings of small circles
fn ring(cx, cy, r, n) {
  let k = 0
  repeat n {
    let a = k * 6.2831853 / n
    circle(cx + cos(a) * r, cy + sin(a) * r, 3)
    k = k + 1
  }
}

let r = 8
repeat 5 {
  color(r * 6, 80, 255 - r * 5)
  ring(50, 50, r, 8 + r)
  r = r + 8
}
`,
  },
  {
    name: "Spiral",
    code: `// a spiral drawn with lines
let a = 0
let r = 2
let px = 50
let py = 50
repeat 120 {
  a = a + 0.35
  r = r + 0.38
  let x = 50 + cos(a) * r
  let y = 50 + sin(a) * r
  color(r * 5, 60, 255 - r * 5)
  line(px, py, x, y)
  px = x
  py = y
}
`,
  },
  {
    name: "Colour grid",
    code: `// a 10 x 10 grid of squares
let row = 0
repeat 10 {
  let col = 0
  repeat 10 {
    color(row * 25, col * 25, 150)
    rect(5 + col * 9, 5 + row * 9, 7, 7)
    col = col + 1
  }
  row = row + 1
}
`,
  },
  {
    name: "Starburst",
    code: `// lines fanning out from the centre
let k = 0
repeat 36 {
  let a = k * 0.1745
  color(k * 7, 100, 255 - k * 7)
  line(50, 50, 50 + cos(a) * 45, 50 + sin(a) * 45)
  k = k + 1
}
`,
  },
  {
    name: "Fibonacci (recursion)",
    code: `// functions and recursion - watch the call stack while stepping
fn fib(n) {
  if n < 2 { return n }
  return fib(n - 1) + fib(n - 2)
}

let i = 0
repeat 10 {
  print(fib(i))
  i = i + 1
}
`,
  },
  {
    name: "Error demo",
    code: `// this program fails at run time - see the error and the highlighted line
let x = 10
print(x)
print(x / (x - 10))
`,
  },
];
