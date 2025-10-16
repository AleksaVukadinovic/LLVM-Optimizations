; ModuleID = 'test_module'
source_filename = "1.ll"

define void @test(i32* %A, i32 %n) {
entry:
  br label %loop.preheader

loop.preheader:              ; Preheader for LICM
  br label %loop

loop:
  %i = phi i32 [0, %loop.preheader], [%i.next, %loop]
  %x = mul i32 %n, 2        ; loop-invariant, should be hoisted
  store i32 %x, i32* %A
  %i.next = add i32 %i, 1
  %cmp = icmp slt i32 %i.next, 10
  br i1 %cmp, label %loop, label %exit

exit:
  ret void
}
