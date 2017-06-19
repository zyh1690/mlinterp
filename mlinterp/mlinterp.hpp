#include <cstddef>
#include <limits>

namespace mlinterp {

	// The user should never call anything in here directly
	namespace detail {

		template <typename T, typename...>
		struct helper {
			template <typename Index>
			void run(const Index *, Index, Index, Index *, T &) {}
		};

		template <typename T, typename T1, typename T2, typename... Args>
		struct helper<T, T1, T2, Args...> : helper<T, Args...> {
			const T *xd;
			const T *xi;

			helper(T1 xd, T2 xi, Args... args) : helper<T, Args...>(args...), xd(xd), xi(xi) {}

			template <typename Index>
			void run(const Index *nd, Index n, Index bitstr, Index *indices, T &factor) {
				const T x = xi[n];

				T weight;
				Index mid;

				if(x <= xd[0]) {
					// Data point is less than left boundary
					mid = 0;
					weight = 1.;
				} else if(x >= xd[*nd - 1]) {
					// Data point is greater than right boundary
					mid = *nd - 2;
					weight = 0.;
				} else {
					// Binary search to find tick
					Index lo = 0, hi = *nd - 2;
					mid = 0;
					weight = 0.;
					while(lo <= hi) {
						mid = lo + (hi - lo) / 2;
						if(x < xd[mid]) { hi = mid - 1; }
						else if(x >= xd[mid + 1]) { lo = mid + 1; }
						else {
							weight = ( xd[mid + 1] - x ) / ( xd[mid + 1] - xd[mid] );
							break;
						}
					}
				}

				if(bitstr & 1) {
					*indices = mid;
					factor *= weight;
				} else {
					*indices = mid + 1;
					factor *= 1 - weight;
				}

				helper<T, Args...> &base = (*this);
				base.run(nd+1, n, bitstr >> 1, indices+1, factor);
			}
		};

	}

	/**
	 * Used to specify a natural order on the grid:
	 * \code{.cpp}
	 * interp<natord>(...)
	 * \endcode
	 */
	struct natord {
		template <typename Index, Index Dimension>
		static Index mux(const Index *nd, const Index *indices) {
			Index index = 0, product = 1, i = 0;
			while(true) {
				index += indices[i] * product;
				if(i == Dimension - 1) { break; }
				product *= nd[i++];
			}
			return index;
		}
	};

	/**
	 * Used to specify a "reverse" natural order on the grid:
	 * \code{.cpp}
	 * interp<rnatord>(...)
	 * \endcode
	 */
	struct rnatord {
		template <typename Index, Index Dimension>
		static Index mux(const Index *nd, const Index *indices) {
			Index index = 0, product = 1, i = Dimension - 1;
			while(true) {
				index += indices[i] * product;
				if(i == 0) { break; }
				product *= nd[--i];
			}
			return index;
		}
	};

	/**
	 * Performs multilinear interpolation of a function specified at data
	 * points (a.k.a. knots) on a rectilinear grid consisting of axes
	 * numbered 0 to m.
	 * A 1d example is given below:
	 * \code{.cpp}
	 * double xd[] = {-1., 0., 1.};
	 * double yd[] = { 1., 0., 1.};
	 * int nd = sizeof(xd) / sizeof(double);
	 *
	 * double xi[] = {-1., -0.75, -0.5, -0.25, 0., 0.25, 0.5, 0.75, 1.}
	 * int ni = sizeof(xi) / sizeof(double);
	 * double yi[ni]; // Store results in this buffer
	 * interp(&nd, ni, yd, yi, xd, xi);
	 * \endcode
	 * @param nd An array whose k-th element is the number of knots on the
	 *           k-th axis.
	 * @param ni The total number of points we want to interpolate.
	 * @param yd An array containing the value of the function at the data
	 *           points. The order in which the values appear in this array
	 *           is determined by the Order template parameter.
	 * @param yi A buffer which we write the results of the interpolation
	 *           to. This buffer should be of size ni.
	 * @tparam args The last argument should be a list of the form
	 *              xd_0, xi_0, ..., xd_m, xi_m. The array xd_k should be of
	 *              size nd[k] and specify the locations of the knots on the
	 *              k-th axis. The j-th point we wish to interpolate has
	 *              coordinates (xi_0[j], ..., xi_m[j]).
	 * @tparam Order The type of order structure to use. By default, the
	 *               natural order is used, which specifies that the j-th
	 *               element of yd, where the index j is expanded as
	 *               j = j_0 + j_1 * nd[j_0] + ... + j_m * nd[j_0] * ... * nd[j_m]
	 *               corresponds to knot (xd_0[j_0], ..., xd_m[j_m]).
	 */
	template <typename Order = natord, typename... Args, typename T, typename Index>
	static void interp(const Index *nd, Index ni, const T *yd, T *yi, Args... args) {
		// Infer dimension from arguments
		static_assert(sizeof...(Args) % 2 == 0, "needs 4+2*Dimension arguments");
		constexpr Index Dimension = sizeof...(Args)/2;

		// Compute 2^Dimension
		constexpr Index Power = 1 << Dimension;

		// Unpack arguments
		detail::helper<T, Args...> h(args...);

		// Perform interpolation for each point
		Index indices[Dimension]; T factor;
		for(Index n = 0; n < ni; ++n) {
			yi[n] = 0.;
			for(Index bitstr = 0; bitstr < Power; ++bitstr) {
				factor = 1.;
				h.run(nd, n, bitstr, indices, factor);
				if(factor > std::numeric_limits<T>::epsilon()) {
					const Index k = Order::template mux<Index, Dimension>(nd, indices);
					yi[n] += factor * yd[k];
				}
			}
		}
	}

}
