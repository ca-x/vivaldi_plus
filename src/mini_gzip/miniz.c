// Other code...

// Check if old_var is within the valid range of mz_uint8
if (old_var >= 0 && old_var <= 255) {
    mz_uint8 new_var = static_cast<mz_uint8>(old_var);
} else {
    // Set new_var to a default value if old_var is not within the valid range of mz_uint8
    mz_uint8 new_var = 0;
}

// Other code...
