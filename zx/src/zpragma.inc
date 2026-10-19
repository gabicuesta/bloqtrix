// Mapa de memoria:
//   0x8000 ........ código y datos
//   ...  0xF3FF     pila (crece hacia abajo desde 0xF400)
//   0xF400-0xFBFF   tabla de dilatación del motor (2 KB, alineada a 256)
//   0xFC00-0xFD00   tabla de vectores IM 2
//   0xFDFD-0xFDFF   salto a la rutina de interrupción
#pragma output CRT_ORG_CODE = 0x8000
#pragma output REGISTER_SP = 0xF400
#pragma output CLIB_MALLOC_HEAP_SIZE = 0
#pragma output CLIB_STDIO_HEAP_SIZE = 0
#pragma output CLIB_EXIT_STACK_SIZE = 0
