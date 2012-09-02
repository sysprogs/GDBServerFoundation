#pragma once

//! Computes the CRC32 checksum as required by GDB
unsigned CRC32(unsigned initial, const void *pBuffer, size_t size);