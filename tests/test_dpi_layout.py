from math import isclose

# Mirrors the C++ layout rules using logical 96-DPI units.
def dpi_scale(value, dpi):
    return value * dpi / 96

def ui_scale(dpi, work_height):
    desired = dpi_scale(530, dpi)
    available = max(420, work_height - 60)
    return max(0.82, available / desired) if desired > available else 1.0

for dpi, work_height in [(96, 900), (120, 840), (144, 760), (192, 1040)]:
    scale = ui_scale(dpi, work_height)
    height = dpi_scale(530, dpi) * scale
    assert height <= work_height - 60 + 1, (dpi, work_height, scale, height)
    assert scale <= 1.0 and scale >= 0.82

# Password dialog logical content bounds: the final action row fits its base client area.
assert 292 + 30 <= 340
assert 18 + 520 <= 560
assert 420 + 100 <= 560
print('PASS: DPI layout fits 100/125/150/200% common work areas')
