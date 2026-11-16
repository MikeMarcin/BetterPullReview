import ctypes
from ctypes import wintypes
class LOGFONTW(ctypes.Structure):
    _fields_=[('lfHeight',ctypes.c_long),('lfWidth',ctypes.c_long),('lfEscapement',ctypes.c_long),('lfOrientation',ctypes.c_long),('lfWeight',ctypes.c_long),('lfItalic',ctypes.c_byte),('lfUnderline',ctypes.c_byte),('lfStrikeOut',ctypes.c_byte),('lfCharSet',ctypes.c_byte),('lfOutPrecision',ctypes.c_byte),('lfClipPrecision',ctypes.c_byte),('lfQuality',ctypes.c_byte),('lfPitchAndFamily',ctypes.c_byte),('lfFaceName',ctypes.c_wchar*32)]
class NCM(ctypes.Structure):
    _fields_=[('cbSize',ctypes.c_uint),('iBorderWidth',ctypes.c_int),('iScrollWidth',ctypes.c_int),('iScrollHeight',ctypes.c_int),('iCaptionWidth',ctypes.c_int),('iCaptionHeight',ctypes.c_int),('lfCaptionFont',LOGFONTW),('iSmCaptionWidth',ctypes.c_int),('iSmCaptionHeight',ctypes.c_int),('lfSmCaptionFont',LOGFONTW),('iMenuWidth',ctypes.c_int),('iMenuHeight',ctypes.c_int),('lfMenuFont',LOGFONTW),('lfStatusFont',LOGFONTW),('lfMessageFont',LOGFONTW),('iPaddedBorderWidth',ctypes.c_int)]
u=ctypes.windll.user32
n=NCM(); n.cbSize=ctypes.sizeof(NCM)
u.SystemParametersInfoW(0x29, n.cbSize, ctypes.byref(n), 0)
print('SPI_GETNONCLIENTMETRICS message font:', n.lfMessageFont.lfFaceName, 'lfHeight', n.lfMessageFont.lfHeight, 'weight', n.lfMessageFont.lfWeight, '| system dpi', u.GetDpiForSystem())
for dpi in (96,120,144):
    m=NCM(); m.cbSize=ctypes.sizeof(NCM)
    u.SystemParametersInfoForDpi(0x29, m.cbSize, ctypes.byref(m), 0, dpi)
    h=m.lfMessageFont.lfHeight
    print('ForDpi', dpi, m.lfMessageFont.lfFaceName, 'lfHeight', h, '=> %.2f pt' % (-h*72.0/dpi))
s=ctypes.c_int(); u.SystemParametersInfoW(0x004A,0,ctypes.byref(s),0); t=ctypes.c_int(); u.SystemParametersInfoW(0x200A,0,ctypes.byref(t),0)
print('font smoothing', s.value, 'type', t.value, '(2 = ClearType)')
