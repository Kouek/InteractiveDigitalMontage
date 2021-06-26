#pragma once

#include <type_traits>
#include <iostream>
#include <vector>
#include <map>
#include <unordered_set>
#include <algorithm>

namespace Kouek
{
	template<typename T>
	class SparseMat
	{
	private:
		int cols;
		int rows;
		int maxNZElemNum;
		int NZElemNum;
		bool valid;
		int* ref; // for GC
		// To find element in (r, c)
		// search for c in colOfIdx from fstIdxOfRow[r] to fstIdxOfRow[r+1]
		// and get datOfIdx from datOfIdx[i] where colOfIdx[i] == c
		T* datOfIdx;
		int* fstIdxOfRow;
		int* colOfIdx;
	private:
		SparseMat(int rows, int cols, int maxNZElemNum);
		void reference(const SparseMat<T>& right);
		void dereference();
		bool insertToZeroEntry(int row, int col, T val);
		void insertZeroToNZEntry(int row, int idx);
	public:
		// constructor
		SparseMat() : 
			valid(false), ref(nullptr) {};
		SparseMat(SparseMat<T>&& right) { reference(right); }
		~SparseMat() { dereference(); }
		static SparseMat<T> initializeFromVector(
			const std::vector<int>& rows, const std::vector<int>& cols,
			const std::vector<T>& vals, int maxNZElemNum = 0);
		// getter
		int getCols() const { return cols; }
		int getRows() const { return rows; }
		bool isValid() { return valid; }
		// basic
		T at(int row, int col) const;
		bool insert(int row, int col, T val);
		void diagTo(std::vector<T>& d) const;
		// linear solution
		static bool solveInGaussSeidel(const SparseMat<T>& A,
			std::vector<double>& x,
			const std::vector<double>& b,
			double sigma = 0.05, int maxStep = 1000);
		static bool solveInConjugateGradient(const SparseMat<T>& A,
			std::vector<double>& x,
			const std::vector<double>& b,
			double sigma = 0.05, int maxStep = 1000);
		// operator
		SparseMat<T>& operator=(const SparseMat<T>& right);
		bool multiply(std::vector<T>& result, const std::vector<T>& mult) const;
		SparseMat<T> ATA() const;
		SparseMat<T> transpose() const;
		friend std::ostream& operator<<(std::ostream& out, SparseMat<T>& mat)
		{
			using namespace std;
			int idx;
			out << endl << "Sparse Mat {" << endl;
			out << '\t' << "rows: " << mat.rows << endl;
			out << '\t' << "cols: " << mat.cols << endl;
			out << '\t' << "maxElementNum: " << mat.maxNZElemNum << endl;
			out << '\t' << "first non-zero col of each row:" << endl;
			out << "\t  ";
			for (idx = 0; idx < mat.rows; idx++)
				out << mat.fstIdxOfRow[idx] << ' ';
			out << '|' << mat.fstIdxOfRow[idx] << "(just a tail)" << endl;
			out << '\t' << "non-zero cols:" << endl;
			out << "\t  ";
			for (idx = 0; idx < mat.NZElemNum; idx++)
				out << mat.colOfIdx[idx] << ' ';
			out << endl;
			out << '\t' << "non-zero vals:" << endl;
			out << "\t  ";
			for (idx = 0; idx < mat.NZElemNum; idx++)
				out << mat.datOfIdx[idx] << ' ';
			out << endl << "}" << endl;
			return out;
		}

	private:
		static inline void vecAddVec(std::vector<double>& x, const std::vector<double>& add0, const std::vector<double>& add1)
		{
			auto itr = x.begin();
			auto itr0 = add0.begin();
			auto itr1 = add1.begin();
			for (; itr != x.end(); ++itr)
			{
				*itr = *itr0 + *itr1;
				++itr0;
				++itr1;
			}
		}
		static inline void vecMinusVec(std::vector<double>& x, const std::vector<double>& minus0, const std::vector<double>& minus1)
		{
			auto itr = x.begin();
			auto itr0 = minus0.begin();
			auto itr1 = minus1.begin();
			for (; itr != x.end(); ++itr)
			{
				*itr = *itr0 - *itr1;
				++itr0;
				++itr1;
			}
		}
		static inline double vecMultiplyVec(const std::vector<double>& mult0, const std::vector<double>& mult1)
		{
			auto itr0 = mult0.begin();
			auto itr1 = mult1.begin();
			double sum = 0;
			for (; itr0 != mult0.end(); ++itr0, ++itr1)
				sum += (*itr0) * (*itr1);
			return sum;
		}

		static inline void numMultiplyVec(std::vector<double>& x, double k, const std::vector<double>& mult)
		{
			auto itr = x.begin();
			auto itr0 = mult.begin();
			for (; itr != x.end(); ++itr)
			{
				*itr = k * (*itr0);
				++itr0;
			}
		}
	};

	template<typename T>
	SparseMat<T>::SparseMat(int rows, int cols, int maxNZElemNum)
		: valid(false), NZElemNum(0)
	{
		this->rows = rows;
		this->cols = cols;
		this->maxNZElemNum = maxNZElemNum;
		ref = new int;
		*ref = 1;

		datOfIdx = new T[maxNZElemNum];
		fstIdxOfRow = new int[rows + 1];
		memset(fstIdxOfRow, -1, sizeof(int) * (rows + 1));
		colOfIdx = new int[maxNZElemNum];
	}

	template<typename T>
	inline void SparseMat<T>::reference(const SparseMat<T>& right)
	{
		this->cols = right.cols;
		this->rows = right.rows;
		this->maxNZElemNum = right.maxNZElemNum;
		this->NZElemNum = right.NZElemNum;
		this->valid = right.valid;
		this->ref = right.ref;
		(*this->ref)++;

		this->datOfIdx = right.datOfIdx;
		this->fstIdxOfRow = right.fstIdxOfRow;
		this->colOfIdx = right.colOfIdx;
	}

	template<typename T>
	inline void SparseMat<T>::dereference()
	{
		if (ref == nullptr)
			return;
		(*ref)--;
		if (*ref == 0)
		{
			// GC
			delete[] datOfIdx;
			delete[] fstIdxOfRow;
			delete[] colOfIdx;
			delete ref;
		}
	}

	struct lessOfPair
	{
		bool operator()(const std::pair<int, int>& p1, const std::pair<int, int>& p2) const
		{
			if (p1.first < p2.first)
				return true;
			else if (p1.first == p2.first)
				return p1.second < p2.second;
			return false;
		}
	};

	template<typename T>
	inline SparseMat<T> SparseMat<T>::initializeFromVector(
		const std::vector<int>& rows, const std::vector<int>& cols,
		const std::vector<T>& vals, int maxNZElemNum)
	{
		if (rows.size() != cols.size()
			|| cols.size() != vals.size()
			|| !std::is_arithmetic<T>::value)
		{
			SparseMat<T> mat(1, 1, 1);
			return mat;
		}
		int maxRow = *std::max_element(rows.begin(), rows.end()) + 1;
		int maxCol = *std::max_element(cols.begin(), cols.end()) + 1;
		int maxEle = maxNZElemNum <= 0 ? vals.size() : maxNZElemNum;
		SparseMat<T> mat(maxRow, maxCol, maxEle);

		// Since rows, cols, vals can be unordered
		// use std::map to reorder them to spd up assignment
		// the format of the map storage is:
		// map<<row, col>, val>
		std::map<std::pair<int, int>, T, lessOfPair> rowAndColToVal;
		int idx;
		for (idx = 0; idx < rows.size(); idx++)
		{
			// abort zero val
			if (vals[idx] == static_cast<T>(0))
				continue;
			rowAndColToVal[std::pair<int, int>(rows[idx], cols[idx])] = vals[idx];
		}

		// assignment
		int r = -1;
		idx = 0;
		for (auto itr = rowAndColToVal.begin(); itr != rowAndColToVal.end(); ++itr)
		{
			// move r to current row
			// set fstIdxOfRow[r] to idx along the way
			while (r < itr->first.first)
				mat.fstIdxOfRow[++r] = idx;

			// assign one non-zero col and its val
			mat.colOfIdx[idx] = itr->first.second;
			mat.datOfIdx[idx] = itr->second;
			idx++;
		}
		while (r < mat.rows)
			mat.fstIdxOfRow[++r] = idx;// useful in func at(row, col) and so on
		mat.NZElemNum = idx;
		mat.valid = true;

		return mat;
	}

	template<typename T>
	inline bool SparseMat<T>::insertToZeroEntry(int row, int col, T val)
	{
		if (NZElemNum == maxNZElemNum)
			return false;
		int idx;
		for (idx = fstIdxOfRow[row]; idx < fstIdxOfRow[row + 1]; idx++)
			if (colOfIdx[idx] > col)break;

		// right shift
		for (int r = row + 1; r <= rows; r++)
			fstIdxOfRow[r]++;
		memcpy(colOfIdx + idx + 1, colOfIdx + idx, sizeof(int) * (NZElemNum - idx));
		memcpy(datOfIdx + idx + 1, datOfIdx + idx, sizeof(T) * (NZElemNum - idx));

		// assignment
		colOfIdx[idx] = col;
		datOfIdx[idx] = val;
		NZElemNum++;
		return true;
	}

	template<typename T>
	inline void SparseMat<T>::insertZeroToNZEntry(int row, int idx)
	{
		// left shift
		for (int r = row + 1; r <= rows; r++)
			fstIdxOfRow[r]--;
		memcpy(colOfIdx + idx, colOfIdx + idx + 1, sizeof(int) * (NZElemNum - idx - 1));
		memcpy(datOfIdx + idx, datOfIdx + idx + 1, sizeof(int) * (NZElemNum - idx - 1));
		NZElemNum--;
	}

	template<typename T>
	inline T SparseMat<T>::at(int row, int col) const
	{
		if (fstIdxOfRow[row] == fstIdxOfRow[row + 1]) // zero row
			return static_cast<T>(0);
		int start = fstIdxOfRow[row];
		int end = fstIdxOfRow[row + 1];
		for (; start < end; start++)
			if (colOfIdx[start] == col)break;
		if (start == end) // zero col but non-zero row
			return static_cast<T>(0);
		return datOfIdx[start];
	}

	template<typename T>
	inline bool SparseMat<T>::insert(int row, int col, T val)
	{
		if (fstIdxOfRow[row] == fstIdxOfRow[row + 1]) // zero row
			if (val == static_cast<T>(0))return true;
			else return insertToZeroEntry(row, col, val);
		int start = fstIdxOfRow[row];
		int end = fstIdxOfRow[row + 1];
		for (; start < end; start++)
			if (colOfIdx[start] == col)break;
		if (start == end) // zero col but non-zero row
			if (val == static_cast<T>(0))return true;
			else return insertToZeroEntry(row, col, val);
		if (val == static_cast<T>(0)) insertZeroToNZEntry(row, start);
		else datOfIdx[start] = val;
		return true;
	}

	template<typename T>
	inline void SparseMat<T>::diagTo(std::vector<T>& d) const
	{
		int diagLen = cols < rows ? cols : rows;
		d.clear();
		d.resize(diagLen);
		int start, end;
		for (int i = 0; i < diagLen; i++)
		{
			if ((start = fstIdxOfRow[i]) == (end = fstIdxOfRow[i + 1]))
				d[i] = static_cast<T>(0);
			for (; start < end; start++)
				if (colOfIdx[start] == i)break;
			if (start == end)d[i] = static_cast<T>(0);
			else d[i] = datOfIdx[start];
		}
	}

	template<typename T>
	inline bool SparseMat<T>::solveInGaussSeidel(const SparseMat<T>& A,
		std::vector<double>& x,
		const std::vector<double>& b,
		double sigma, int maxStep)
	{
		if (A.getCols() != A.getRows()) // must be square mat
			return false;
		if (A.getRows() != b.size()) // row of A must equal to size of x and b
			return false;
		if (sigma <= 0.0)sigma = 0.05;
		if (maxStep <= 0)maxStep = 1000;

		// initialization
		x.clear();
		x.resize(b.size(), 0);
		std::vector<T> diag;
		A.diagTo(diag);

		// start iteration
		int i, j;
		double sum;
		bool isCov;
		int rows = A.getRows(), cols = A.getCols();
		for (int step = 0; step < maxStep; step++)
		{
			isCov = true;
			for (i = 0; i < rows; i++)
			{
				sum = 0.0;
				for (j = 0; j < cols; j++)
				{
					if (i == j)continue;
					sum += x[j] * A.at(i, j);
				}
				sum = (b[i] - sum) / diag[i];
				if (abs(sum - x[i]) > sigma) // not coverenced
					isCov = false;
				x[i] = sum;
			}
			if (isCov)break;
		}
		return true;
	}

	template<typename T>
	inline bool SparseMat<T>::solveInConjugateGradient(const SparseMat<T>& A, std::vector<double>& x, const std::vector<double>& b, double sigma, int maxStep)
	{
		if (A.getCols() != A.getRows()) // must be square mat
			return false;
		if (A.getRows() != b.size()) // row of A must equal to size of x and b
			return false;
		if (sigma <= 0.0)sigma = 0.05;
		if (maxStep <= 0)maxStep = 1000;

		// initialization
		std::vector<double> Ax0;
		A.multiply(Ax0, x);
		std::vector<double> r0(b);
		vecMinusVec(r0, r0, Ax0);
		std::vector<double> p0(r0);

		// start iteration
		int rows = A.getRows(), cols = A.getCols();
		double a, beta, r0r0;
		std::vector<double> Ap(x.size()), p(x.size());
		for (int step = 0; step < maxStep; step++)
		{
			r0r0 = vecMultiplyVec(r0, r0);
			A.multiply(Ap, p0);
			a = r0r0 / vecMultiplyVec(p0, Ap);

			numMultiplyVec(p, a, p0);
			vecAddVec(x, x, p);

			numMultiplyVec(p, a, Ap);
			vecMinusVec(r0, r0, p);
			a = 0;
			for (double ele : r0)
				a += ele * ele;
			if (sqrt(a) <= sigma)break;

			beta = vecMultiplyVec(r0, r0);
			beta /= r0r0;

			numMultiplyVec(p0, beta, p0);
			vecAddVec(p0, r0, p0);
		}
		return true;
	}

	template<typename T>
	inline SparseMat<T>& SparseMat<T>::operator=(const SparseMat<T>& right)
	{
		dereference();
		reference(right);
		return *this;
	}

	template<typename T>
	inline bool SparseMat<T>::multiply(std::vector<T>& result, const std::vector<T>& mult) const
	{
		if (cols != mult.size())
			return false;
		if (result.size() != rows)
			result.resize(rows);
		int row = 0, idx = 0;
		for (row = 0; row < rows; row++)
		{
			result[row] = static_cast<T>(0);
			if (fstIdxOfRow[row] == fstIdxOfRow[row + 1])
				continue;
			for (idx = fstIdxOfRow[row]; idx < fstIdxOfRow[row + 1]; idx++)
				result[row] += mult[colOfIdx[idx]] * datOfIdx[idx];
		}
		return true;
	}

	template<typename T>
	inline SparseMat<T> SparseMat<T>::ATA() const
	{
		// << A r*c = [
		//      a00, a01, ..., a0(n-1)
		//      a10, a11, ..., a1(n-1)
		//      ...
		//    ]
		// << AT c*r = [
		//      a00, a10, ..., a(n-1)0
		//      a01, a11, ..., a(n-1)1
		//      ...
		//    ]
		// >> ATA c*c [i,j] = 
		//      a0i*a0j + a1i*a1j + ..., + a(n-1)i*a(n-1)j

		// calc AT first,
		// since a0i a1i a2i ... are of one row in AT
		// instead of one col in A
		SparseMat<double> AT = this->transpose();

		SparseMat<T> mat(this->cols, this->cols, this->maxNZElemNum);

		T* vals = new T[this->cols];

		int r = -1;
		int idx = 0;
		for (int i = 0; i < this->cols; i++)
		{
			if (AT.fstIdxOfRow[i] == AT.fstIdxOfRow[i + 1])
				continue; // this row is 0
			memset(vals, static_cast<T>(0), sizeof(T) * this->cols);

			int j;
			for (int ki = AT.fstIdxOfRow[i]; ki < AT.fstIdxOfRow[i + 1]; ki++)
			{
				j = AT.colOfIdx[ki];
				for (int kj = this->fstIdxOfRow[j]; kj < this->fstIdxOfRow[j + 1]; kj++)
				{
					vals[this->colOfIdx[kj]] += AT.datOfIdx[ki] * this->datOfIdx[kj];
				}
			}

			for (int j = 0; j < this->cols; j++)
			{
				if (vals[j] == static_cast<T>(0))
					continue;
				// move r to current row
				// set fstIdxOfRow[r] to idx along the way
				while (r < i)
					mat.fstIdxOfRow[++r] = idx;

				// assign one non-zero col and its val
				mat.colOfIdx[idx] = j;
				mat.datOfIdx[idx] = vals[j];
				idx++;

				if (idx == mat.maxNZElemNum)
				{
					// deal with space inadequate problem
					T* newTPtr = new T[mat.maxNZElemNum * 2];
					memcpy(newTPtr, mat.datOfIdx, sizeof(T) * idx);
					delete[] mat.datOfIdx;
					mat.datOfIdx = newTPtr;

					int* newIntPtr = new int[mat.maxNZElemNum * 2];
					memcpy(newIntPtr, mat.colOfIdx, sizeof(int) * idx);
					delete[] mat.colOfIdx;
					mat.colOfIdx = newIntPtr;

					mat.maxNZElemNum *= 2;
				}
			}
		}
		delete[] vals;

		while (r < mat.rows)
			mat.fstIdxOfRow[++r] = idx;// useful in func at(row, col) and so on
		mat.NZElemNum = idx;
		mat.valid = true;
		return mat;
#if 0
		T val;
		int r = -1;
		int idx = 0;
		for (int i = 0; i < cols; i++)
		{
			if (AT.fstIdxOfRow[i] == AT.fstIdxOfRow[i + 1])
				continue; // this row is 0

			for (int j = 0; j < cols; j++)
			{
				val = static_cast<T>(0);

				if (AT.fstIdxOfRow[j] == AT.fstIdxOfRow[j + 1])
					continue; // this col is 0
				if (AT.colOfIdx[AT.fstIdxOfRow[i + 1] - 1] < AT.colOfIdx[AT.fstIdxOfRow[j]])
					continue; // no intersection
				if (AT.colOfIdx[AT.fstIdxOfRow[j + 1] - 1] < AT.colOfIdx[AT.fstIdxOfRow[i]])
					continue; // no intersection

				int ki, kj;
				for (
					ki = AT.fstIdxOfRow[i], kj = AT.fstIdxOfRow[j];
					ki < AT.fstIdxOfRow[i + 1];
					ki++)
				{
					if (AT.colOfIdx[ki] < AT.colOfIdx[kj])
						continue; // next ki until ki match kj
					else if (AT.colOfIdx[ki] > AT.colOfIdx[kj])
					{
						while (
							AT.colOfIdx[ki] > AT.colOfIdx[kj]
							&& kj != AT.fstIdxOfRow[j + 1]
							)
							kj++; // next kj until ki match kj
						if (kj >= AT.fstIdxOfRow[j + 1])
							break; // no more kj match ki
						else if (AT.colOfIdx[ki] == AT.colOfIdx[kj])
							val += AT.datOfIdx[ki] * AT.datOfIdx[kj]; // ki match kj
					}
					else
						val += AT.datOfIdx[ki] * AT.datOfIdx[kj]; // ki match kj
				}

				if (val == static_cast<T>(0))
					continue; // this computed result is 0

				// move r to current row
				// set fstIdxOfRow[r] to idx along the way
				while (r < i)
					mat.fstIdxOfRow[++r] = idx;

				// assign one non-zero col and its val
				mat.colOfIdx[idx] = j;
				mat.datOfIdx[idx] = val;
				idx++;

				if (idx == mat.maxNZElemNum)
				{
					// deal with space inadequate problem
					T* newTPtr = new T[mat.maxNZElemNum * 2];
					memcpy(newTPtr, mat.datOfIdx, sizeof(T) * idx);
					delete[] mat.datOfIdx;
					mat.datOfIdx = newTPtr;

					int* newIntPtr = new int[mat.maxNZElemNum * 2];
					memcpy(newIntPtr, mat.colOfIdx, sizeof(int) * idx);
					delete[] mat.colOfIdx;
					mat.colOfIdx = newIntPtr;

					mat.maxNZElemNum *= 2;
				}
			}
		}

		while (r < mat.rows)
			mat.fstIdxOfRow[++r] = idx;// useful in func at(row, col) and so on
		mat.NZElemNum = idx;
		mat.valid = true;
		return mat;
#endif
	}

	template<typename T>
	inline SparseMat<T> SparseMat<T>::transpose() const
	{
		SparseMat<T> mat(cols, rows, maxNZElemNum);

		std::map<std::pair<int, int>, T, lessOfPair> rowAndColToVal;
		for (int row = 0; row < rows; row++)
			for (int idx = fstIdxOfRow[row]; idx < fstIdxOfRow[row + 1]; idx++)
				rowAndColToVal[std::pair<int, int>(colOfIdx[idx], row)] = datOfIdx[idx];

		// assignment
		int r = -1;
		int idx = 0;
		for (auto itr = rowAndColToVal.begin(); itr != rowAndColToVal.end(); ++itr)
		{
			// move r to current row
			// set fstIdxOfRow[r] to idx along the way
			while (r < itr->first.first)
				mat.fstIdxOfRow[++r] = idx;

			// assign one non-zero col and its val
			mat.colOfIdx[idx] = itr->first.second;
			mat.datOfIdx[idx] = itr->second;
			idx++;
		}
		while (r < mat.rows)
			mat.fstIdxOfRow[++r] = idx;// useful in func at(row, col) and so on
		mat.NZElemNum = idx;
		mat.valid = true;

		return mat;
	}
}