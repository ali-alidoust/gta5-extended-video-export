#pragma once

template <class T>
int pvfIndex(T func)
{
	union {
		T pfn;
		unsigned char* pb;
	};
	pfn = func;
	if (!pb) return -1;
	int pboff = -1;
	if (pb[0] == 0x8b && pb[1] == 0x01) {	//mov eax, [ecx]
		pboff = 2;
	}
	else if (pb[0] == 0x8b && pb[1] == 0x44 && pb[2] == 0x24 && pb[3] == 0x04 &&		//mov eax, [esp+arg0]
		pb[4] == 0x8b && pb[5] == 0x00) {										//mov eax, [eax]
		pboff = 6;
	}

	if (pboff > 0) {
		if (pb[pboff] == 0xff) {
			switch (pb[pboff + 1]) {
			case 0x20:	//jmp dword ptr [eax]
				return 0;
			case 0x60:	//jmp dword ptr [eax+0xNN]
				return (((int)pb[pboff + 2]) & 0xff) / 4;
			case 0xa0:	//jmp dword ptr [eax+0xNNN]
				return (*(unsigned int*)(pb + (pboff + 2))) / 4;
			default:
				break;
			}
		}
	}
	return -1;
}