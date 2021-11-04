/*
 * pthreads.c 
 * Convert a square N x N matrix into the CSR format, made for sparse matrices:
 * https://en.wikipedia.org/wiki/Sparse_matrix#Compressed_sparse_column_(CSC_or_CCS)
 * 
 * Authors: Antonios Antoniou - 9482
 *          Efthymios Grigorakis - 9694
 *    
 * 2021 Aristotle University of Thessaloniki
 * Parallel and Distributed Systems.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define SIZE 5
#define MAX_THREADS 1000000

// A struct used to turn sparse matrices to CSR data structures.
// The notation and algorithm used is taken directly from the given Wikipedia page.
typedef struct {
	long size;
	int *values;
	long *colIndex;
	long *rowIndex;
} csr;


int **makeRandomSparseTable(int size) {
	int **table = (int **) malloc(size * sizeof(int *));
	for (int i = 0; i < size; i++) {
		table[i] = (int *) malloc(size * sizeof(int));
	}

	// Only add values to the upper triangle of the table.
	for (int i = 0; i < size; i++) {
		for (int j = i; j < size; j++) {
			int newValue = rand() % 1000;

			if (newValue < 750) {
				table[i][j] = 0;
			} else if (newValue >= 750 && newValue <= 930) {
				table[i][j] = 1;
			} else {
				table[i][j] = 2;
			}
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


// Prints a CSR data structure.
void printCSR(csr converted, long size) {
	long nonzeros = converted.rowIndex[size];
	
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


void printTable(int **table, int size) {
	for (int i = 0; i < size; i++) {
		for (int j = 0; j < size; j++) {
			printf(" %d ", table[i][j]);
		}
		printf("\n");
	}

	printf("\n");
}


// Converts a sparse matrix to CSR.
csr matrixToCSR(int **table, long size) {
	// Keep track of the nonzero objects.
	long nonzeros = 0; 

	// Initialize the 3 necessary arrays. row_index has a standard length of "rows+1"
	long *rowIndex = (long *) malloc((size+1) * sizeof(long));
	// values and col_index have a length of the total nonzero values.
	int *values = (int *) malloc(100 * sizeof(int));
	long *colIndex = (long *) malloc(100 * sizeof(long));
	
	for (long i = 0; i < size; i++) {
		// Add the nonzero values that are placed above the current row. 
		rowIndex[i] = nonzeros;

		for (long j = 0; j < size; j++) {
			if (table[i][j] != 0) {
				nonzeros++;

				// Make sure to extend the arrays in case their size exceeds the current one.
				if (nonzeros % 100 == 0) {
					values = (int *) realloc(values, (nonzeros+100) * sizeof(int));
					colIndex = (long *) realloc(colIndex, (nonzeros+100) * sizeof(long));
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

	csr converted = {size, values, colIndex, rowIndex};
	return converted;
}


int **CSRtoMatrix(csr table, long size) {
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
  for (long row = 0; row < size; row++) {
    long start = table.rowIndex[row];
    long end = table.rowIndex[row+1];

    for (int j = start; j < end; j++) {
      long column = table.colIndex[j];
      int value = table.values[j];
      matrix[row][column] = value;
    }
  }

  // Done.
  return matrix;
}


// Matrix multiplication ONLY FOR SQUARE MATRICES.
int **matmul (int **table1, int **table2, int size) {
	int **multTable = (int **) malloc(size * sizeof(int*));

	for (int i = 0; i < size; i++) {
		multTable[i] = (int *) malloc(size * sizeof(int));
	}

	// Set the table values to 0 to avoid garbage initializations. 
	for (int i = 0; i < size; i++) {
		for (int j = 0; j < size; j++) {
			multTable[i][j] = 0;
		}
	}

	for (int i = 0; i < size; i++) {
		for (int j = 0; j < size; j++) {
			for (int k = 0; k < size; k++) {
				multTable[i][j] += table1[i][k] * table2[k][j];
			}
		}
	}

	return multTable;
}

// Calculates the square of CSR matrix, as long as it's square.
csr csrSquare(csr converted, int **table, long size) {
	long nonzeros = converted.values[size];

	// The new values array. Intialize all to 0. Do the same for the new column indices.
	int *newValues = (int *) malloc(10 * size * sizeof(int));
	long *newColIndex = (long *) malloc(10 * size * sizeof(long));

	for (long i = 0; i < 10 * nonzeros; i++) {
		newValues[i] = 0;
		newColIndex[i] = 0;
	}

	// Finally, initialize the new row index array.
	long *newRowIndex = (long *) malloc((size+1) * sizeof(long));
	for (long i = 0; i < size+1; i++) {
		newRowIndex[i] = 0;
	}

	// Keep the new matrix's total of nonzero values.
	long newNonzeros = 0;
	// Keep tab of how many times the newValues and colIndex arrays have been resized.
	int resizes = 10; 

	// Scan every row using the row_index array.
	for (long row = 0; row < size; row++) {
		if (newNonzeros != 0 && (newNonzeros % (10*nonzeros) == 0)) {
			resizes += 10;
			newValues = (int *) realloc(newValues, size * resizes * sizeof(int));
			newColIndex = (long *) realloc(newColIndex, size * resizes * sizeof(long));
		}

		newRowIndex[row] = newNonzeros;
		// printf("Row: %ld\t Nonzeros = %ld\n", row, newRowIndex[row]);

		// Get the indices of the values that lie within each row.
		long start = converted.rowIndex[row];
		long end = converted.rowIndex[row+1];
		// printf("Start = %ld\t End = %ld\n", start, end);

		for (long column = 0; column < size; column++) {
			long cellValue = 0;

			for (long element = start; element < end; element++) {
				cellValue += converted.values[element] * table[converted.colIndex[element]][column];
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
	return product;
}


csr hadamard(csr csrTable, int **square, long size) {
  long nonzeros = csrTable.rowIndex[size];

  long *newRowIndex = (long *) malloc((size+1) * sizeof(long));
  long *newColIndex = (long *) malloc(nonzeros * sizeof(long));
  int *newValues = (int *) malloc(nonzeros * sizeof(int));
  long newNonzeros = 0;

  for (int i = 0; i < nonzeros; i++) {
    newColIndex[i] = 0;
    newValues[i] = 0;
  }

  for (int i = 0; i < size; i++) {
    newRowIndex[i] = 0;
  }

  for (long row = 0; row < size; row++) {
    newRowIndex[row] = newNonzeros;

    long start = csrTable.rowIndex[row];
    long end = csrTable.rowIndex[row+1];

    for (int i = start; i < end; i++) {
      long column = csrTable.colIndex[i];
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



int main(int argc, char **argv) {
	int **random1 = makeRandomSparseTable(SIZE);
	csr converted = matrixToCSR(random1, SIZE);
	csr squareCSR = csrSquare(converted, random1, SIZE);

  int **squareMatrix = CSRtoMatrix(squareCSR, SIZE);

  csr C = hadamard(converted, squareMatrix, SIZE);

  printTable(random1, SIZE);
  printTable(squareMatrix, SIZE);
  printCSR(squareCSR, SIZE);

  // printTable(CSRtoMatrix(C, SIZE), SIZE);
	
	return 0;
}