#ifndef HELPERS_H
#define HELPERS_H

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <limits.h>

#include "../headers/mmio.h"
#include "../headers/csr.h"
#include "../headers/helpers.h"


// Reads an mtx file and returns the CSR format by making a 2D array of non-symmetric 
// dimensions. Stores the column indices in each row that corresponds to a row in the
// valuesByRow array. Then increases the size of each sub array by one, since most
// vertices are connected to few other vertices.
csr readmtx_dynamic(char *mtx, MM_typecode t, int N, int M, int nz) {
  FILE *matrixFile = fopen(mtx, "r");
	int banner = mm_read_banner(matrixFile, &t);
	int result = mm_read_mtx_crd_size(matrixFile, &M, &N, &nz);

	printf("\nbanner: %d\tresult: %d\tnonzeros: %d\tM: %d\tN: %d\n",
		banner, result, nz, M, N);
 
	// Display error messages and abort, if the matrix isn't square or hasn't been read properly.
	if (N != M) {
		printf("N and M are not equal. The matrix isn't square. Aborting...");
		csr returnError = {0, NULL, NULL, NULL};
		return returnError;
	}

	if (banner != 0 || result != 0) {
		printf("Error. Couldn't process the .mtx file!");
		csr returnError = {0, NULL, NULL, NULL};
		return returnError;	
	}

  int **valuesByRow = (int **) malloc(N * sizeof(int *));
  for (int i = 0; i < N; i++) {
    valuesByRow[i] = (int *) calloc(2, sizeof(int));
  }

  uint nonzeros = 0;
  for (int i = 0; i < nz; i++) {
    int row, col;
    fscanf(matrixFile , "%d  %d \n", &row, &col);
    // Decrease the values since Matlab is 1-index based. 
    row--;
    col--;

    int newEntry = valuesByRow[row][0] + 1;
    valuesByRow[row][newEntry] = col;
    valuesByRow[row][0]++;
    valuesByRow[row] = (int *) realloc(valuesByRow[row], (valuesByRow[row][0] + 2) * sizeof(int));
    nonzeros++;

    if (row != col) {
      int newEntry = valuesByRow[col][0] + 1;
      valuesByRow[col][newEntry] = row;
      valuesByRow[col][0]++;
      valuesByRow[col] = (int *) realloc(valuesByRow[col], (valuesByRow[col][0] + 2) * sizeof(int));
      nonzeros++;
    }
  }

  // This is a binary matrix. All the nonzero values are 1 by default.
  int *values = (uint *) malloc(nonzeros *sizeof(int));
  for (int i = 0; i < nonzeros; i++) {
    values[i] = 1;
  }


  uint *rowIndex = (uint *) calloc(N, sizeof(uint));
  uint *colIndex = (uint *) calloc(nonzeros, sizeof(uint));

  for (uint row = 0; row < N; ++row) {
    int rowNonzeros = valuesByRow[row][0];

    for (int column = 1; column < rowNonzeros+1; column++) {
      uint currentNonzeros = rowIndex[row + 1];

      colIndex[currentNonzeros] = valuesByRow[row][column];
      rowIndex[row + 1]++;
    }

    if (row < N - 1) {
      rowIndex[row + 2] = rowIndex[row + 1];
    }
  }

  csr csr_mtx = {N, values, colIndex, rowIndex};
  return csr_mtx;
}


csr hadamardSingleStep(csr table, uint start, uint end) {
  uint size = end - start;
  
	uint nonzeros = table.rowIndex[end] - table.rowIndex[start];
	uint newNonzeros = 0;
	// The new values array. Intialize all to 0. Do the same for the new column indices.
	int *newValues = (int *) calloc(nonzeros, sizeof(int));
	uint *newColIndex = (uint *) calloc(nonzeros, sizeof(uint));

	// Finally, initialize the new row index array.
	uint *newRowIndex = (uint *) calloc((size+1), sizeof(uint));

  printf("Initialized the csr arrays\n");

  // Find the values in A^2, iff the original matrix had a nonzero in that position.
	for (uint row = start; row < end; row++) {
    uint rowStart = table.rowIndex[row];
    uint rowEnd = table.rowIndex[row+1];

    for (uint index = rowStart; index < rowEnd; index++) {
      uint currentColumn = table.colIndex[index];

      int value = dot(table, row, currentColumn);

      if (value > 0) {
        newValues[newNonzeros] = value;
        newColIndex[newNonzeros] = currentColumn;
        newRowIndex[row+1]++;

        newNonzeros++;
      }
    }

    // Pass the next value if we haven't reached the last row.
    if (row < size - 1) {
      newRowIndex[row+2] = newRowIndex[row+1];
    }
	}

  // newValues = (int *) realloc(newValues, newNonzeros * sizeof(int));
  // newColIndex = (uint *) realloc(newValues, newNonzeros * sizeof(uint));

  csr hadamard = {size, newValues, newColIndex, newRowIndex};
  return hadamard;
}


// Calculates the dot product of two vectors.
int dot(csr table, uint row, uint column) {
  // Symmetric table. Rows are identical to columns and vice versa.
  uint rowStart = table.rowIndex[row];
  uint rowEnd = table.rowIndex[row+1];

  uint colStart = table.rowIndex[column];
  uint colEnd = table.rowIndex[column+1];

  int value = 0;
  // Leaves a trace of the last match. The next iteratio will start from
  // the nexr position to get rid of unnecessary comparisons.
  uint lastMatch = colStart - 1;

  for (uint i = rowStart; i < rowEnd; i++) {
    for (uint j = lastMatch+1; j < colEnd; j++) {
      if (table.colIndex[i] == table.colIndex[j]) {
        value += table.values[i] * table.values[j];
        break;
      }
    }
  }

  return value;
}


uint *countTriangles(csr C) {
	uint size = C.size;
	uint *triangleCount = (uint *) malloc(size * sizeof(uint));
	for (uint i = 0; i < size; i++) {
		triangleCount[i] = 0;
	}

	// Add all the values in each row, then divide by 2.
	// Simulates the operation of multiplying the table with a nx1 vector.
	for (uint i = 0; i < size; i++) {
		uint start = C.rowIndex[i];
		uint end = C.rowIndex[i+1];

		for (uint j = start; j < end; j++) {
			triangleCount[i] += C.values[j];
			
			// Divide by 2 if we reached the last value of the current row.
			if (j == end - 1) {
				triangleCount[i] /= 2;
			}
		}
	}

	return triangleCount;
}


// Matrix multiplication. Only need rows1, cols1 and cols2, because
// cols1==rows2 is required. The new matrix is of size rows1 x cols2.
int **matmul (int **table1, int **table2, uint rows1, uint cols1, uint cols2) {
	int **multTable = (int **) malloc(rows1 * sizeof(int*));

	for (int i = 0; i < rows1; i++) {
		multTable[i] = (int *) malloc(rows1 * sizeof(int));
	}

	// Set the table values to 0 to avoid garbage initializations. 
	for (int i = 0; i < rows1; i++) {
		for (int j = 0; j < cols2; j++) {
			multTable[i][j] = 0;
		}
	}

	for (int i = 0; i < rows1; i++) {
		for (int j = 0; j < cols2; j++) {
			for (int k = 0; k < cols1; k++) {
				multTable[i][j] += table1[i][k] * table2[k][j];
			}
		}
	}

	return multTable;
}


// Calculates the square of CSR matrix, as uint as it's square.
// FINAL VERSION OF THE FUNCTION.
csr csrSquare(csr table, uint size) {
	uint nonzeros = table.rowIndex[size];
	int resizes = 5;
	uint newNonzeros = 0;

	// The new values array. Intialize all to 0. Do the same for the new column indices.
	int *newValues = (int *) malloc(resizes * size * sizeof(int));
	uint *newColIndex = (uint *) malloc(resizes * size * sizeof(uint));
	for (uint i = 0; i < resizes * size; i++) {
		newValues[i] = 0;
		newColIndex[i] = 0;
	}

	// Finally, initialize the new row index array.
	uint *newRowIndex = (uint *) malloc((size+1) * sizeof(uint));
	for (uint i = 0; i < size+1; i++) {
		newRowIndex[i] = 0;
	}

	for (uint row = 0; row < size; row++) {
		newRowIndex[row] = newNonzeros;
		uint start = table.rowIndex[row];
		uint end = table.rowIndex[row+1];

		// Scan and multiply -only nonzero elements- every single column. Since the matrix is
		// symmetric, we actually scan every line from top to bottom,
		// getting exactly the same result.
		for (uint column = start; column < end; column++) {

			// Make sure to check the table for resizes regularly.
			if (newNonzeros == resizes * size - 1) {
				resizes += 5;
				newValues = (int *) realloc(newValues, size * resizes * sizeof(int));
				newColIndex = (uint *) realloc(newColIndex, size * resizes * sizeof(uint));
			}

			int cellValue = dot(table, row, table.colIndex[column]);

			// uint columnStart = table.rowIndex[column];
			// uint columnEnd = table.rowIndex[column+1];

			// // This variable is used to leave a trace at the last element where the row-column pair
			// // was a match, for the element-wise multiplication to take place. Since the order here 
			// // is increasing, we leave a trace where the last match was found and the scan 
			// // for another matching pair starts at the next index.
			// int lastMatch = columnStart - 1;

			// for (uint rowElement = start; rowElement < end; rowElement++) {
			// 	for (uint colElement = lastMatch; colElement < columnEnd; colElement++) {
			// 		if (table.colIndex[colElement] == table.colIndex[rowElement]) {
			// 			lastMatch = colElement; // Mark the last match.

			// 			// Add the product to the cell value.
			// 			cellValue += table.values[colElement] * table.values[rowElement];
			// 		}
			// 	}
			// }

			if (cellValue > 0) {
				// printf("Cell value: %ld\n", cellValue);
				newValues[newNonzeros] = cellValue;
				newColIndex[newNonzeros] = column;
				newNonzeros++;
			}
		}

		// Add the final value to newRowIndex before exiting the loop.
		if (row == size - 1) {
			newRowIndex[size] = newNonzeros;
		}
	}

	// Resize the arrays to save space and avoid null values.
	newValues = (int *) realloc(newValues, newNonzeros * sizeof(int));
	newColIndex = (uint *) realloc(newColIndex, newNonzeros * sizeof(uint));

	csr square;
	square.size = size;
	square.values = newValues;
	square.colIndex = newColIndex;
	square.rowIndex = newRowIndex;
	return square;
}


// CSR-CSR Hadamard (element-wise) operation.
// FINAL VERSION OF THE FUNCTION.
csr newhadamard(csr csrTable, csr square, uint size) {
	uint oldNonzeros = csrTable.rowIndex[size];
	uint newNonzeros = 0;

	uint *newColIndex = (uint *) malloc(oldNonzeros * sizeof(uint));
	int *newValues = (int *) malloc(oldNonzeros * sizeof(int));
	for (int i = 0; i < oldNonzeros; i++) {
		newColIndex[i] = 0;
		newValues[i] = 0;
	}

	uint *newRowIndex = (uint *) malloc((size+1) * sizeof(uint));
	for (int i = 0; i < size+1; i++) {
		newRowIndex[i] = 0;
	}

	// Use the old table and only transfer values to the new one if they are greater than 0.
	for (int i = 0; i < size; i++) {
		newRowIndex[i] = newNonzeros;

		for (int j = csrTable.rowIndex[i]; j < csrTable.rowIndex[i+1]; j++) {
			for (int k = square.rowIndex[i]; k < square.rowIndex[i+1]; k++) {
				if(csrTable.colIndex[j] == square.colIndex[k]) {
					csrTable.values[j] = square.values[k];

					newValues[newNonzeros] = csrTable.values[j];
					newColIndex[newNonzeros] = csrTable.colIndex[j];
					newNonzeros++;
					
					break;
				}
			}
		}

		if (i == size-1) {
			newRowIndex[size] = newNonzeros;
		}
	}

	// Resize the arrays to save space and avoid null values.
	newValues = (int *) realloc(newValues, newNonzeros * sizeof(int));
	newColIndex = (uint *) realloc(newColIndex, newNonzeros * sizeof(uint));

	csr hadamard;
	hadamard.size = size;
	hadamard.values = newValues;
	hadamard.colIndex = newColIndex;
	hadamard.rowIndex = newRowIndex;
	return hadamard;
}


// Reads an .mtx file and outputs the resulting CSR form.
csr readmtx(char *mtx, MM_typecode t, int N, int M, int nz) {
	FILE *matrixFile = fopen(mtx, "r");
	int banner = mm_read_banner(matrixFile, &t);
	int result = mm_read_mtx_crd_size(matrixFile, &M, &N, &nz);

	printf("\nbanner: %d\tresult: %d\tnonzeros: %d\tM: %d\tN: %d\n",
		banner, result, nz, M, N);
 
	// Display error messages and abort, if the matrix isn't square or hasn't been read properly.
	if (N != M) {
		printf("N and M are not equal. The matrix isn't square. Aborting...");
		csr returnError = {0, NULL, NULL, NULL};
		return returnError;
	}

	if (banner != 0 || result != 0) {
		printf("Error. Couldn't process the .mtx file!");
		csr returnError = {0, NULL, NULL, NULL};
		return returnError;	
	}

	int *row, *col;
	row = (int *) malloc(2 * nz * sizeof(int));	
	col = (int *) malloc(2 * nz * sizeof(int));

	for(int i = 0; i < nz; i++) {
		fscanf(matrixFile , "%d  %d \n", &row[i], &col[i]);
		// Decrease the values since Matlab is 1-based. 
		row[i]--;
		col[i]--;

		// Add the symmetric values, since the .mtx files contains only half the matrix.
		row[i+nz] = col[i];
		col[i+nz] = row[i];

    if (i % (N / 10) == 0){
      printf("Read %ld up until now.\n", i);
    }

	}
	
	csr Table;
	Table.size = N;
	Table.rowIndex = (uint *) malloc((M+1) * sizeof(uint));
	Table.colIndex = (uint *) malloc(2 * nz * sizeof(uint));
	Table.values = (int *) malloc(2 * nz * sizeof(int));

	int currentEntry = 0;

	for (int i = 0; i < M; i++) {
		for (int j = 0; j < 2*nz; j++) {
			if (row[j] == i) {
				Table.rowIndex[i+1]++;
				Table.values[currentEntry] = 1;
				Table.colIndex[currentEntry] = col[j];

				currentEntry++;
			}

      // Pass the current total of nonzeros to the next row.
			Table.rowIndex[i+2] = Table.rowIndex[i+1];
		}

    if (i % (N / 10) == 0){
      printf("Stored %ld rows up until now.\n", i);
    }
	}

	return Table;
}


csr csrSquareAlt(csr converted, int **table, uint size) {
	uint nonzeros = converted.values[size];

	// The new values array. Intialize all to 0. Do the same for the new column indices.
	int *newValues = (int *) malloc(10 * size * sizeof(int));
	uint *newColIndex = (uint *) malloc(10 * size * sizeof(uint));
	for (uint i = 0; i < 10 * nonzeros; i++) {
		newValues[i] = 0;
		newColIndex[i] = 0;
	}

	// Finally, initialize the new row index array.
	uint *newRowIndex = (uint *) malloc((size+1) * sizeof(uint));
	for (uint i = 0; i < size+1; i++) {
		newRowIndex[i] = 0;
	}

	// Keep the new matrix's total of nonzero values.
	uint newNonzeros = 0;
	// Keep tab of how many times the newValues and colIndex arrays have been resized.
	int resizes = 10; 

	// Scan every row using the row_index array.
	for (uint row = 0; row < size; row++) {
		// Make sure to resize the arrays if they're full.
		if (newNonzeros != 0 && (newNonzeros % (10*nonzeros) == 0)) {
			resizes += 10;
			newValues = (int *) realloc(newValues, size * resizes * sizeof(int));
			newColIndex = (uint *) realloc(newColIndex, size * resizes * sizeof(uint));
		}

		newRowIndex[row] = newNonzeros;
		// printf("Row: %ld\t Nonzeros = %ld\n", row, newRowIndex[row]);

		// Get the indices of the values that lie within each row.
		uint start = converted.rowIndex[row];
		uint end = converted.rowIndex[row+1];
		// printf("Start = %ld\t End = %ld\n", start, end);

		for (uint column = 0; column < size; column++) {
			int cellValue = 0;

			for (uint element = start; element < end; element++) {
				uint elementRow = converted.colIndex[element];
				cellValue += converted.values[element] * table[elementRow][column];
			}

			if (cellValue > 0) {
				// printf("Cell value: %ld\n", cellValue);
				newValues[newNonzeros] = cellValue;
				newColIndex[newNonzeros] = column;
				newNonzeros++;
			}
		}

		// Add the final value to newRowIndex before exiting the loop.
		if (row == size - 1) {
			newRowIndex[size] = newNonzeros;
		}
	}

	csr product = {size, newValues, newColIndex, newRowIndex};
	free(newValues);
	free(newColIndex);
	free(newRowIndex);
	return product;
}


csr hadamard(csr csrTable, int **square, uint size) {
	uint nonzeros = csrTable.rowIndex[size];

	uint *newRowIndex = (uint *) malloc((size+1) * sizeof(uint));
	uint *newColIndex = (uint *) malloc(nonzeros * sizeof(uint));
	int *newValues = (int *) malloc(nonzeros * sizeof(int));
	uint newNonzeros = 0;

	for (int i = 0; i < nonzeros; i++) {
		newColIndex[i] = 0;
		newValues[i] = 0;
	}

	for (int i = 0; i < size; i++) {
		newRowIndex[i] = 0;
	}

	for (uint row = 0; row < size; row++) {
		newRowIndex[row] = newNonzeros;

		uint start = csrTable.rowIndex[row];
		uint end = csrTable.rowIndex[row+1];

		for (int i = start; i < end; i++) {
			uint column = csrTable.colIndex[i];
			int value = csrTable.values[i];

			int cellValue = value * square[row][column];
			if (cellValue > 0) {
				newValues[newNonzeros] = cellValue;
				newColIndex[newNonzeros] = column;
				newNonzeros++;
			}
		}

		// Add the final value to newRowIndex before exiting the loop.
		if (row == size - 1) {
			newRowIndex[size] = newNonzeros;
		}
	}

	csr hadamard = {size, newValues, newColIndex, newRowIndex};
	return hadamard;
}




// Converts a sparse matrix to CSR.
csr matrixToCSR(int **table, uint size) {
	// Keep track of the nonzero objects.
	uint nonzeros = 0; 
	uint resizes = 10;

	// Initialize the 3 necessary arrays. row_index has a standard length of "rows+1"
	uint *rowIndex = (uint *) malloc((size+1) * sizeof(uint));
	for (int i = 0; i < size+1; i++) {
		rowIndex[i] = 0;
	}

	// values and col_index have a length of the total nonzero values.
	int *values = (int *) malloc(10 * size * sizeof(int));
	uint *colIndex = (uint *) malloc(10 * size * sizeof(uint));
	for (int i = 0; i < 10*size; i++) {
		values[i] = 0;
		colIndex[i] = 0;
	}
	
	for (uint i = 0; i < size; i++) {
		// Add the nonzero values that are placed above the current row. 
		rowIndex[i] = nonzeros;

		for (uint j = 0; j < size; j++) {
			if (table[i][j] != 0) {
				nonzeros++;

				// Make sure to extend the arrays in case their size exceeds the current one.
				if (nonzeros == resizes * size -1) {
					resizes += 10;
					values = (int *) realloc(values, (resizes * size) * sizeof(int));
					colIndex = (uint *) realloc(colIndex, (resizes * size) * sizeof(uint));
				}

				// Add the nonzero value and the respective column index.
				values[nonzeros-1] = table[i][j];
				colIndex[nonzeros-1] = j; 
			}
		}

		// Add the last value before exiting the for loop.
		if (i == size - 1) {
			rowIndex[size] = nonzeros;
		}
	}

	csr converted;
	converted.size = size;
	converted.values = values;
	converted.colIndex = colIndex;
	converted.rowIndex = rowIndex;
	return converted;
}


int **CSRtoMatrix(csr table, uint size) {
	// Initialize the new matrix and set everything to 0.
	int **matrix = (int **) malloc(size * sizeof(int*));
	for (int i = 0; i < size; i++) {
		matrix[i] = (int *) malloc(size * sizeof(int));
	}

	for (int i = 0; i < size; i++) {
		for (int j = 0; j < size; j++) {
			matrix[i][j] = 0;
		}
	}

	// Scan each row for nonzero values and plug them into the matrix.
	for (uint row = 0; row < size; row++) {
		uint start = table.rowIndex[row];
		uint end = table.rowIndex[row+1];

		for (int j = start; j < end; j++) {
			uint column = table.colIndex[j];
			int value = table.values[j];
			matrix[row][column] = value;
		}
	}

	// Done.
	return matrix;
}


/**
 * ---------- HELPERS ----------
 */ 
// Prints a CSR data structure.
void printCSR(csr converted) {
  uint size = converted.size;
	uint nonzeros = converted.rowIndex[size];
	
	printf("Values:");
	for (int i = 0; i < nonzeros; i++) {
		printf(" %d ", converted.values[i]);
	}

	printf("\nCol_index:");
	for (int i = 0; i < nonzeros; i++) {
		printf(" %ld ", converted.colIndex[i]);
	}

	printf("\nRow_index:");
	for (int i = 0; i < size+1; i++) {
		printf(" %ld ", converted.rowIndex[i]);
	}
	printf("\n\n");
}


int **makeRandomSparseTable(int size) {
	int **table = (int **) malloc(size * sizeof(int *));
	for (int i = 0; i < size; i++) {
		table[i] = (int *) malloc(size * sizeof(int));
	}

	// Only add values to the upper triangle of the table.
	for (int i = 0; i < size; i++) {
		for (int j = i; j < size; j++) {
			table[i][j] = (rand() % 1000 < 750) ? 0 : 1; 

			// ALTERNATIVE INITIALIZATION. USE 2's INSTEAD
			// OF ONLY ONES AND ZEROS.

			// int newValue = rand() % 1000;
			// if (newValue < 750) {
			// 	table[i][j] = 0;
			// } else if (newValue >= 750 && newValue <= 930) {
			// 	table[i][j] = 1;
			// } else {
			// 	table[i][j] = 2;
			// }
		}
	}

	// Make it symmetric by setting A[i][j] = A[j][i] for the rest of the values.
	for (int i = 0; i < size; i++) {
		for (int j = 0; j < i; j++) {
			table[i][j] = table[j][i];
		}
	}

	return table;
}


void printTable(int **table, uint size) {
	for (uint i = 0; i < size; i++) {
		for (uint j = 0; j < size; j++) {
			printf(" %d ", table[i][j]);
		}
		printf("\n");
	}

	printf("\n");
}

#endif