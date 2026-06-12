// Stub: shadow clang's built-in <stdatomic.h> to prevent conflict with <atomic>
// C++ code should use <atomic> instead of C11 <stdatomic.h>
#pragma once
// This file intentionally empty — prevents compiler's stdatomic.h from leaking
// kill_dependency, atomic_is_lock_free and other C11 atomic macros
