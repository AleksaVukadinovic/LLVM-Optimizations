  %sum = alloca i32, align 4
  store i32 0, i32* %sum, align 4
  ; i = 0
  %0 = load i32, i32* %sum
  %1 = add i32 %0, 0
  store i32 %1, i32* %sum
  %2 = load i32, i32* %sum
  %3 = add i32 %2, 1
  store i32 %3, i32* %sum
  %4 = load i32, i32* %sum
  %5 = add i32 %4, 2
  store i32 %5, i32* %sum
  %6 = load i32, i32* %sum
  %7 = add i32 %6, 3
  store i32 %7, i32* %sum
