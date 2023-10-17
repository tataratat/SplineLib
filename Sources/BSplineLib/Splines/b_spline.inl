/* Copyright (c) 2018–2021 SplineLib

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

// TODO(all): use NamedInteger::ForEach once clang supports capturing of
// variables from structured bindings.

template<int parametric_dimensionality>
BSpline<parametric_dimensionality>::BSpline() : Base_(false) {}

template<int parametric_dimensionality>
BSpline<parametric_dimensionality>::BSpline(
    SharedPointer<ParameterSpace_> parameter_space,
    SharedPointer<VectorSpace_> vector_space)
    : Base_(std::move(parameter_space), false) {
  using std::to_string;

#ifndef NDEBUG
  int const &total_number_of_basis_functions =
                Base_::parameter_space_->GetTotalNumberOfBasisFunctions(),
            &number_of_coordinates = vector_space->GetNumberOfCoordinates();
  if (number_of_coordinates != total_number_of_basis_functions)
    Throw(DomainError(to_string(number_of_coordinates)
                      + " coordinates were provided but "
                      + to_string(total_number_of_basis_functions)
                      + " are needed to associate each basis function "
                        "with a coordinate."),
          "bsplinelib::splines::BSpline::BSpline");
#endif
  vector_space_ = std::move(vector_space);
}

template<int parametric_dimensionality>
BSpline<parametric_dimensionality>::BSpline(BSpline const& other)
    : Base_(other),
      vector_space_{std::make_shared<VectorSpace_>(*other.vector_space_)} {}

template<int parametric_dimensionality>
BSpline<parametric_dimensionality>&
BSpline<parametric_dimensionality>::operator=(BSpline const& rhs) {
  Base_::operator=(rhs),
  vector_space_ = std::make_shared<VectorSpace_>(*rhs.vector_space_);
  return *this;
}

template<int parametric_dimensionality>
void BSpline<parametric_dimensionality>::Evaluate(
    const DataType_* parametric_coordinate,
    DataType_* evaluated) const {
  // view array for evaluated. used in recursive combine
  Array_<DataType_> evaluated_b_spline(evaluated, vector_space_->Dim());

  ParameterSpace_ const& parameter_space = *Base_::parameter_space_;

  const auto basis_per_dim =
      parameter_space.EvaluateBasisValuesPerDimension(parametric_coordinate);
  const auto beginning =
      parameter_space.FindFirstNonZeroBasisFunction(parametric_coordinate);
  auto offset = parameter_space.First();

  bsplinelib::parameter_spaces::RecursiveCombine(
      basis_per_dim,
      beginning,
      offset,
      vector_space_->GetCoordinates(),
      evaluated_b_spline);
}

template<int parametric_dimensionality>
void BSpline<parametric_dimensionality>::EvaluateDerivative(
    const DataType_* parametric_coordinate,
    const IndexType_* derivative,
    DataType_* evaluated) const {
  // create view for return
  Coordinate_ evaluated_b_spline_derivative(evaluted, vector_space->Dim());

  ParameterSpace_ const& parameter_space = *Base_::parameter_space_;

  const auto basis_derivative_per_dim =
      parameter_space.EvaluateBasisDerivativeValuesPerDimension(
          parametric_coordinate,
          derivative);
  const auto beginning =
      parameter_space.FindFirstNonZeroBasisFunction(parametric_coordinate);
  auto offset = parameter_space.First();

  bsplinelib::parameter_spaces::RecursiveCombine(
      basis_derivative_per_dim,
      beginning,
      offset,
      vector_space_->GetCoordinates(),
      evaluated_b_spline_derivative);
}

template<int parametric_dimensionality>
typename Spline<parametric_dimensionality>::Coordinate_
BSpline<parametric_dimensionality>::operator()(
    ParametricCoordinate_ const& parametric_coordinate) const {
  Coordinate_ evaluated_b_spline(vector_space_->Dim());
  Evaluate(parametric_coordinate.data(), evaluted_b_spline.data());
  return evaluated_b_spline;
}

template<int parametric_dimensionality>
typename Spline<parametric_dimensionality>::Coordinate_
BSpline<parametric_dimensionality>::operator()(
    ParametricCoordinate_ const& parametric_coordinate,
    Derivative_ const& derivative) const {
  Coordinate_ evaluated_b_spline_derivative(vector_space->Dim());
  EvaluateDerivative(parametric_coordinate.data(),
                     derivative.data(),
                     evaluated_b_spline_derivative.data());
  return evaluated_b_spline_derivative;
}

// Cf. NURBS book Eq. (5.15).
template<int parametric_dimensionality>
void BSpline<parametric_dimensionality>::InsertKnot(
    Dimension const& dimension,
    Knot_ knot,
    Multiplicity const& multiplicity,
    Tolerance const& tolerance) const {
  using utilities::containers::Add;

  ParameterSpace_& parameter_space = *Base_::parameter_space_;
  Dimension::Type_ const& dimension_value = dimension.Get();
#ifndef NDEBUG
  Message const kName{"bsplinelib::splines::BSpline::InsertKnot"};

  try {
    Dimension::ThrowIfNamedIntegerIsOutOfBounds(dimension,
                                                parametric_dimensionality - 1);
    utilities::numeric_operations::ThrowIfToleranceIsNegative(tolerance);
    parameter_space.ThrowIfParametricCoordinateIsOutsideScope(dimension,
                                                              knot,
                                                              tolerance);
  } catch (InvalidArgument const& exception) {
    Throw(exception, kName, dimension_value);
  } catch (OutOfRange const& exception) {
    Throw(exception, kName, dimension_value);
  }

#endif
  VectorSpace_& vector_space = *vector_space_;
  IndexLength_ number_of_coordinates{
      parameter_space.GetNumberOfBasisFunctions()};
  auto const& [start_value, coefficients] =
      parameter_space.InsertKnot(dimension, knot, multiplicity, tolerance);

  // create tmp_coord to hold new coordinate values
  Array_<DataType_> tmp_coord(vector_space.Dim());
  for (KnotRatios_ const& current_coefficients : coefficients) {
    IndexLength_ number_of_coordinates_in_slice{number_of_coordinates};
    number_of_coordinates_in_slice[dimension_value] = Length{};
    IndexLength_ const previous_number_of_coordinates{number_of_coordinates};
    ++number_of_coordinates[dimension_value];
    for (Index_ slice_coordinate{Index_::First(number_of_coordinates_in_slice)};
         slice_coordinate != Index_::Behind(number_of_coordinates_in_slice);
         ++slice_coordinate) {
      constexpr KnotRatio_ const k1_0{1.0};

      IndexValue_ coordinate_value{slice_coordinate.GetIndex()};
      coordinate_value[dimension_value] = start_value;
      Index_ coordinate{number_of_coordinates, coordinate_value};
      typename KnotRatios_::const_reverse_iterator coefficient{
          current_coefficients.rbegin()};
      Index const insertion_position = coordinate.GetIndex1d();

      Add(*coefficient,
          vector_space.CoordinateBegin(
              (slice_coordinate + coordinate_value).GetIndex1d().Get()),
          k1_0 - *coefficient,
          vector_space.CoordinateBegin(
              coordinate.Decrement(dimension).GetIndex1d().Get()),
          tmp_coord);
      vector_space.ReallocateInsert(insertion_position, tmp_coord);
      // vector_space.Insert(
      //     insertion_position,
      //     Add(Multiply(vector_space[Index_{previous_number_of_coordinates,
      //                                      coordinate_value}
      //                                   .GetIndex1d()
      //                               + slice_coordinate.GetIndex1d()],
      //                  *coefficient),
      //         Multiply(vector_space[coordinate.Decrement(dimension)
      //                                   .GetIndex1d()],
      //                  k1_0 - *coefficient)));
      ++coefficient;
      for (; coefficient < current_coefficients.rend(); ++coefficient) {
        Index const& replacement_position = coordinate.GetIndex1d();
        Add(*coefficient,
            vector_space.CoordinateBegin(replacement_position.Get()),
            k1_0 - *coefficient,
            vector_space.CoordinateBegin(
                coordinate.Decrement(dimension).GetIndex1d().Get()),
            tmp_coord);
        vector_space.Replace(replacement_position, tmp_coord);
        // vector_space.Replace(
        //     replacement_position,
        //     Add(Multiply(vector_space[replacement_position], *coefficient),
        //         Multiply(vector_space[coordinate.Decrement(dimension)
        //                                   .GetIndex1d()],
        //                  k1_0 - *coefficient)));
      }
    }
  }
}

template<int parametric_dimensionality>
Multiplicity BSpline<parametric_dimensionality>::RemoveKnot(
    Dimension const& dimension,
    Knot_ const& knot,
    Tolerance const& tolerance_removal,
    Multiplicity const& multiplicity,
    Tolerance const& tolerance) const {

  Dimension::Type_ const& dimension_value = dimension.Get();
#ifndef NDEBUG
  using utilities::numeric_operations::ThrowIfToleranceIsNegative;

  Message const kName{"bsplinelib::splines::BSpline::RemoveKnot"};

  try {
    Dimension::ThrowIfNamedIntegerIsOutOfBounds(dimension,
                                                parametric_dimensionality - 1);
    ThrowIfToleranceIsNegative(tolerance_removal);
    ThrowIfToleranceIsNegative(tolerance);
  } catch (InvalidArgument const& exception) {
    Throw(exception, kName, dimension_value);
  } catch (OutOfRange const& exception) {
    Throw(exception, kName, dimension_value);
  }
#endif
  // dereference and gather infos for the loop
  ParameterSpace_& parameter_space = *Base_::parameter_space_;
  ParameterSpace_ parameter_space_backup{parameter_space};
  IndexLength_ number_of_coordinates{
      parameter_space.GetNumberOfBasisFunctions()};
  auto const& [start_value, coefficients] =
      parameter_space.RemoveKnot(dimension, knot, multiplicity, tolerance);
  Multiplicity::Type_ const& removals = coefficients.size();

  // create tmp coord to process coefficient multiplications
  // replacement is a view
  const int vector_space_dim = vector_space_->Dim();
  Array_ replacement_coord;
  replacement_coord.SetShape(vector_space_dim);
  // comparison is owning array
  Array_ comparison_value(vector_space_dim);
  for (Multiplicity removal{removals}; removal > Multiplicity{}; --removal) {
    VectorSpace_& vector_space = *vector_space_;
    VectorSpace_ const vector_space_backup{vector_space};
    KnotRatios_ const& current_coefficients = coefficients[removal.Get() - 1];
    IndexLength_ number_of_coordinates_in_slice{number_of_coordinates};
    number_of_coordinates_in_slice[dimension_value] = Length{};
    IndexLength_ const previous_number_of_coordinates{number_of_coordinates};
    --number_of_coordinates[dimension_value];
    for (Index_ slice_coordinate{Index_::Last(number_of_coordinates_in_slice)};
         slice_coordinate != Index_::Before(number_of_coordinates_in_slice);
         --slice_coordinate) {
      constexpr KnotRatio_ const k1_0{1.0};

      IndexValue_ coordinate_value{slice_coordinate.GetIndex()};
      coordinate_value[dimension_value] =
          (start_value
           - Index{static_cast<Index::Type_>(current_coefficients.size())});
      Index_ coordinate{previous_number_of_coordinates, coordinate_value};
      Index coordinate_index{coordinate.GetIndex1d()}, lower_coordinate_index;
      typename KnotRatios_::const_iterator coefficient{
          current_coefficients.begin()};
      for (; coefficient != std::prev(current_coefficients.end());
           ++coefficient) {
        KnotRatio_ const& current_coefficient = *coefficient;
        lower_coordinate_index = coordinate_index;
        coordinate_index = coordinate.Increment(dimension).GetIndex1d();
        // replace
        replacement_coord.SetData(
            vector_space.CoordinateBegin(coordinate_index.Get()));
        replacement_coord.MultiplyAssign(
            (k1_0 - current_coefficient).Get(),
            vector_space.CoordinateBegin(lower_coordinate_index.Get()));
        replacement_coord.FlipSubtract(
            vector_space.CoordinateBegin(coordinate_index.Get()));
        replacement_coord.Multiply(1. / current_coefficient);

        // vector_space.Replace(
        //     coordinate_index,
        //     Divide(Subtract(vector_space[coordinate_index],
        //                     Multiply(vector_space[lower_coordinate_index],
        //                              k1_0 - current_coefficient)),
        //            current_coefficient));
      }
      KnotRatio_ const& current_coefficient = *coefficient;
      lower_coordinate_index = coordinate_index;
      coordinate_index = coordinate.Increment(dimension).GetIndex1d();
      // compute value to compare
      comparison_value.MultiplyAssign(
          (k1_0 - current_coefficient).Get(),
          vector_space.CoordinateBegin(lower_coordinate_index.Get()));
      comparison_value.FlipSubstract(
          vector_space.CoordinateBegin(coordinate_index.Get()));
      comparison_value.Multiply(1. / current_coefficient.Get());
      const Index subtact_id =
          (slice_coordinate + coordinate.GetIndex()).GetIndex1d() + Index{1};
      comparison_value.Subtract(
          vector_space.CoordinateBegin(subtract_id.Get()));
      // if (utilities::std_container_operations::EuclidianDistance(
      //         Divide(Subtract(vector_space[coordinate_index],
      //                         Multiply(vector_space[lower_coordinate_index],
      //                                  k1_0 - current_coefficient)),
      //                current_coefficient),
      //         vector_space[Index_{number_of_coordinates,
      //         coordinate.GetIndex()}
      //                          .GetIndex1d()
      //                      + slice_coordinate.GetIndex1d() + Index{1}])
      //     <= tolerance_removal) {
      if (comparison_value.NormL2() <= tolerance_removal) {
        vector_space.Erase(coordinate_index);
      } else {
        Multiplicity const& successful_removals = (multiplicity - removal);
        parameter_space_backup.RemoveKnot(dimension,
                                          knot,
                                          successful_removals,
                                          tolerance);
        parameter_space = parameter_space_backup;
        vector_space = vector_space_backup;
        return successful_removals;
      }
    }
  }
  return Multiplicity{removals};
}

// Cf. NURBS book Eq. (5.36).
template<int parametric_dimensionality>
void BSpline<parametric_dimensionality>::ElevateDegree(
    Dimension const& dimension,
    Multiplicity const& multiplicity,
    Tolerance const& tolerance) const {
  using utilities::std_container_operations::AddAndAssignToFirst,
      utilities::std_container_operations::GetBack,
      utilities::std_container_operations::Multiply;

  Dimension::Type_ const& dimension_value = dimension.Get();
#ifndef NDEBUG
  Message const kName{"bsplinelib::splines::BSpline::ElevateDegree"};

  try {
    Dimension::ThrowIfNamedIntegerIsOutOfBounds(dimension,
                                                parametric_dimensionality - 1);
    utilities::numeric_operations::ThrowIfToleranceIsNegative(tolerance);
  } catch (InvalidArgument const& exception) {
    Throw(exception, kName, dimension_value);
  } catch (OutOfRange const& exception) {
    Throw(exception, kName, dimension_value);
  }
#endif
  ParameterSpace_& parameter_space = *Base_::parameter_space_;
  VectorSpace_& vector_space = *vector_space_;
  const int vector_space_dim = vector_space.Dim();
  // allocate an array to insert
  Array_ coordinate(vector_space_dim);
  Array_ tmp_coordinate(vector_space_dim);
  tmp_coordinate.SetShape(vector_space_dim);
  auto const& [number_of_segments, knots_inserted] =
      MakeBezier(dimension, tolerance);
  IndexLength_ number_of_coordinates{
      parameter_space.GetNumberOfBasisFunctions()};
  auto const& [last_segment_coordinate, coefficients] =
      parameter_space.ElevateDegree(dimension, multiplicity);
  IndexLength_ number_of_coordinates_in_slice{number_of_coordinates};
  number_of_coordinates_in_slice[dimension_value] = Length{};
  for (int segment{}; segment < number_of_segments; ++segment) {
    Index const maximum_interior_coordinate{
        static_cast<int>(coefficients.size()) - 1};
    Index interior_coordinate{maximum_interior_coordinate},
        last_coordinate{((segment + 1) * last_segment_coordinate.Get())
                        + (segment * multiplicity.Get())};
    for (; interior_coordinate >= (last_segment_coordinate - Index{1});
         --interior_coordinate) {
      BinomialRatios_ const& current_coefficients =
          coefficients[interior_coordinate.Get()];
      IndexLength_ const previous_number_of_coordinates{number_of_coordinates};
      ++number_of_coordinates[dimension_value];
      for (Index_ slice_coordinate{
               Index_::First(number_of_coordinates_in_slice)};
           slice_coordinate != Index_::Behind(number_of_coordinates_in_slice);
           ++slice_coordinate) {
        IndexValue_ coordinate_value{slice_coordinate.GetIndex()};
        coordinate_value[dimension_value] = last_coordinate;
        Index_ current_coordinate{number_of_coordinates, coordinate_value};
        Index const& insertion_position = current_coordinate.GetIndex1d();
        IndexValue_ current_last_coordinate_value{coordinate_value};
        current_last_coordinate_value[dimension_value] +=
            (maximum_interior_coordinate - interior_coordinate);

        // compute coordinate to insert
        coordinate.MultiplyAssign(
            GetBack(current_coefficients).Get(),
            vector_space.CoordinateBegin(
                (slice_coordinate + current_last_coordinate_value)
                    .GetIndex1d()
                    .Get()));

        // Coordinate_ coordinate{
        //     Multiply(vector_space[Index_{previous_number_of_coordinates,
        //                                  current_last_coordinate_value}
        //                               .GetIndex1d()
        //                           + slice_coordinate.GetIndex1d()],
        //              GetBack(current_coefficients))};
        std::for_each(current_coefficients.rbegin() + 1,
                      current_coefficients.rend(),
                      [&](BinomialRatio_ const& coefficient) {
                        tmp_coordinate.MultiplyAssign(
                            coefficient.Get(),
                            vector_space.CoordinateBegin(
                                current_coordinate.Decrement(dimension)
                                    .GetIndex1d()
                                    .Get()));

                        coordinate.Add(tmp_coordinate);

                        // AddAndAssignToFirst(
                        //     coordinate,
                        //     Multiply(vector_space[current_coordinate.Decrement(dimension)
                        //                               .GetIndex1d()],
                        //              coefficient));
                      });
        vector_space.ReallocateInsert(insertion_position.Get(), coordinate);
      }
    }
    for (; interior_coordinate >= Index{}; --interior_coordinate) {
      BinomialRatios_ const& current_coefficients =
          coefficients[interior_coordinate.Get()];
      --last_coordinate;
      for (Index_ slice_coordinate{
               Index_::First(number_of_coordinates_in_slice)};
           slice_coordinate != Index_::Behind(number_of_coordinates_in_slice);
           ++slice_coordinate) {
        IndexValue_ coordinate_value{slice_coordinate.GetIndex()};
        coordinate_value[dimension_value] = last_coordinate;
        Index_ current_coordinate{number_of_coordinates, coordinate_value};
        Index const& replacement_position = current_coordinate.GetIndex1d();

        // get replacement coordinate and work on it directly
        coordinate.SetData(
            vector_space.CoordinateBegin(replacement_position.Get()));
        coordinate.Multiply(GetBack(current_coefficients).Get());
        // Coordinate_ coordinate{Multiply(vector_space[replacement_position],
        //                                 GetBack(current_coefficients))};
        std::for_each(current_coefficients.rbegin() + 1,
                      current_coefficients.rend(),
                      [&](BinomialRatio_ const& coefficient) {
                        tmp_coordinate.MultiplyAssign(
                            coefficient.Get(),
                            vector_space.CoordinateBegin(
                                current_coordinate.Decrement(dimension)
                                    .GetIndex1d()
                                    .Get()));

                        coordinate.Add(tmp_coordinate);
                        // AddAndAssignToFirst(
                        //     coordinate,
                        //     Multiply(vector_space[current_coordinate.Decrement(dimension)
                        //                               .GetIndex1d()],
                        //              coefficient));
                      });
        // vector_space.Replace(replacement_position, coordinate);
      }
    }
  }
  Base_::CoarsenKnots(dimension, knots_inserted, tolerance);
}

template<int parametric_dimensionality>
bool BSpline<parametric_dimensionality>::ReduceDegree(
    Dimension const& dimension,
    Tolerance const& tolerance_reduction,
    Multiplicity const& multiplicity,
    Tolerance const& tolerance) const {
  using std::for_each, utilities::std_container_operations::GetBack;

  Dimension::Type_ const& dimension_value = dimension.Get();
#ifndef NDEBUG
  using utilities::numeric_operations::ThrowIfToleranceIsNegative;

  Message const kName{"bsplinelib::splines::BSpline::ReduceDegree"};

  try {
    Dimension::ThrowIfNamedIntegerIsOutOfBounds(dimension,
                                                parametric_dimensionality - 1);
    ThrowIfToleranceIsNegative(tolerance_reduction);
    ThrowIfToleranceIsNegative(tolerance);
  } catch (InvalidArgument const& exception) {
    Throw(exception, kName, dimension_value);
  } catch (OutOfRange const& exception) {
    Throw(exception, kName, dimension_value);
  }
#endif
  ParameterSpace_& parameter_space = *Base_::parameter_space_;
  ParameterSpace_ parameter_space_backup{parameter_space};
  VectorSpace_& vector_space = *vector_space_;
  VectorSpace_ vector_space_backup{vector_space};
  const int vector_space_dim = vector_space.Dim();
  auto const& [number_of_segments, knots_inserted] =
      MakeBezier(dimension, tolerance);
  IndexLength_ number_of_coordinates{
      parameter_space.GetNumberOfBasisFunctions()};
  auto const& [last_segment_coordinate, coefficients] =
      parameter_space.ReduceDegree(dimension, multiplicity);
  Degree::Type_ const& elevatetd_degree = (coefficients.size() + 1);
  IndexLength_ number_of_coordinates_in_slice{number_of_coordinates};
  number_of_coordinates_in_slice[dimension_value] = Length{};
  // array
  Array_ view_coordinate;
  view_coordinate.SetShape(vector_space_dim);
  Array_ tmp_coordinate(vector_space_dim);
  for (int segment{number_of_segments - 1}; segment >= 0; --segment) {
    Index interior_coordinate{},
        coordinate_index{1 + (segment * elevatetd_degree)};
    for (; interior_coordinate < (last_segment_coordinate - Index{1});
         ++interior_coordinate) {
      BinomialRatios_ const& current_coefficients =
          coefficients[interior_coordinate.Get()];
      for (Index_ slice_coordinate{
               Index_::Last(number_of_coordinates_in_slice)};
           slice_coordinate != Index_::Before(number_of_coordinates_in_slice);
           --slice_coordinate) {
        IndexValue_ coordinate_value{slice_coordinate.GetIndex()};
        coordinate_value[dimension_value] = coordinate_index;
        Index_ current_coordinate{number_of_coordinates, coordinate_value};
        Index const& replacement_position = current_coordinate.GetIndex1d();
        // Coordinate_ coordinate{vector_space[replacement_position]};
        view_coordinate.SetData(
            vector_space.CoordinateBegin(replacement_position.Get()));

        for_each(current_coefficients.begin(),
                 std::prev(current_coefficients.end()),
                 [&](BinomialRatio_ const& coefficient) {
                   tmp_coordinate.MultiplyAssign(
                       coefficient.Get(),
                       vector_space.CoordinateBegin(
                           current_coordinate.Decrement(dimension)
                               .GetIndex1d()
                               .Get()));
                   view_coordinate.Subtract(tmp_coordinate);
                   // SubtractAndAssignToFirst(
                   //     coordinate,
                   //     Multiply(vector_space[current_coordinate.Decrement(dimension)
                   //                               .GetIndex1d()],
                   //              coefficient));
                 });

        view_coordinate.Multiply(1. / GetBack(current_coefficients).Get());
        // vector_space.Replace(replacement_position,
        //                      utilities::std_container_operations::Divide(
        //                          coordinate,
        //                          GetBack(current_coefficients)));
      }
      ++coordinate_index;
    }
    Index const maximum_interior_coordinate{elevatetd_degree - 2};
    for (; interior_coordinate <= maximum_interior_coordinate;
         ++interior_coordinate) {
      BinomialRatios_ const& current_coefficients =
          coefficients[interior_coordinate.Get()];
      IndexLength_ const previous_number_of_coordinates{number_of_coordinates};
      --number_of_coordinates[dimension_value];
      for (Index_ slice_coordinate{
               Index_::Last(number_of_coordinates_in_slice)};
           slice_coordinate != Index_::Before(number_of_coordinates_in_slice);
           --slice_coordinate) {
        IndexValue_ coordinate_value{slice_coordinate.GetIndex()};
        coordinate_value[dimension_value] = coordinate_index;
        Index_ current_coordinate{previous_number_of_coordinates,
                                  coordinate_value};
        Index const& erasure_position = current_coordinate.GetIndex1d();
        IndexValue_ current_last_coordinate_value{coordinate_value};
        current_last_coordinate_value[dimension_value] +=
            (maximum_interior_coordinate - interior_coordinate);
        // Coordinate_ coordinate{vector_space[erasure_position]};

        // so, this point will be removed if the removal works. We will just use
        // this
        view_coordinate.SetData(
            vector_space.CoordinateBegin(erasure_position.Get()));
        for_each(current_coefficients.rbegin() + 1,
                 current_coefficients.rend(),
                 [&](BinomialRatio_ const& coefficient) {
                   tmp_coordinate.MultiplyAssign(
                       coefficient.Get(),
                       vector_space.CoordinateBegin(
                           current_coordinate.Decrement(dimension)
                               .GetIndex1d()
                               .Get()));
                   view_coordinate.Subtract(tmp_coordinate);
                   // SubtractAndAssignToFirst(
                   //     coordinate,
                   //     Multiply(vector_space[current_coordinate.Decrement(dimension)
                   //                               .GetIndex1d()],
                   //              coefficient));
                 });
        view_coordinate.Multiply(1. / GetBack(current_coefficients).Get());
        view_coordinate.Subtract(vector_space.CoordinateBegin(
            ((slice_coordinate + current_last_coordinate_value).GetIndex1d()
             + Index{1})
                .Get()));

        if (view_coordinate.NormL2() <= tolerance_reduction) {
          // if ( // IsLessOrEqual(
          //     utilities::std_container_operations::EuclidianDistance(
          //         utilities::std_container_operations::DivideAndAssignToFirst(
          //             coordinate,
          //             GetBack(current_coefficients)),
          //         vector_space[Index_{number_of_coordinates,
          //                             current_last_coordinate_value}
          //                          .GetIndex1d()
          //                      + slice_coordinate.GetIndex1d() + Index{1}])
          //     <= tolerance_reduction) {
          vector_space.Erase(erasure_position.Get());
        } else {
          parameter_space = parameter_space_backup;
          vector_space = vector_space_backup;
          return false;
        }
      }
    }
  }
  Base_::CoarsenKnots(dimension, knots_inserted, tolerance);
  return true;
}

template<int parametric_dimensionality>
Coordinate BSpline<parametric_dimensionality>::
    ComputeUpperBoundForMaximumDistanceFromOrigin(
        Tolerance const& tolerance) const {
  return vector_space_->DetermineMaximumDistanceFromOrigin(tolerance);
}

// See NURBS book p. 169.
template<int parametric_dimensionality>
typename BSpline<parametric_dimensionality>::BezierInformation_
BSpline<parametric_dimensionality>::MakeBezier(
    Dimension const& dimension,
    Tolerance const& tolerance) const {
  BezierInformation_ const& bezier_information =
      Base_::parameter_space_->DetermineBezierExtractionKnots(dimension,
                                                              tolerance);
  Base_::RefineKnots(dimension, std::get<1>(bezier_information));
  return bezier_information;
}
