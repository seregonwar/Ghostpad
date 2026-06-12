# ps4-kernel
- For ucred offset discovery on PS4, use the vda_probe technique: hardcoded reference offsets verified via read-compare (kernel_getlong vs kernel_get_ucred_authid) before writing. Avoid blind byte-scanning of kernel structs — prefer manifest-style read-verify-write with a narrow probe window around known PS5 SDK offsets. Confidence: 0.75

# architecture
- On PS4, never execute PS5Bridge or any PS5 payload code at startup — detect platform via CPUID and skip all PS5-specific code paths entirely when running on PS4. Confidence: 0.85

# Taste (Continuously Learned by [CommandCode][cmd])

[cmd]: https://commandcode.ai/

