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

template<int dimensionality>
WeightedVectorSpace<dimensionality>::WeightedVectorSpace(
    Coordinates_ const& coordinates,
    Weights_ const& weights)
    : Base_{HomogenizeCoordinates(coordinates, weights)} {}

template<int dimensionality>
typename WeightedVectorSpace<dimensionality>::Coordinate_
WeightedVectorSpace<dimensionality>::Project(
    HomogeneousCoordinate_ const& homogeneous_coordinate) {
  Coordinate_ coordinate;
  std::transform(homogeneous_coordinate.begin(),
                 std::prev(homogeneous_coordinate.end()),
                 coordinate.begin(),
                 std::bind(std::multiplies{},
                           std::placeholders::_1,
                           static_cast<typename Coordinate_::value_type>(1.)
                               / homogeneous_coordinate[dimensionality]));
  return coordinate;
}

template<int dimensionality>
typename WeightedVectorSpace<
    dimensionality>::MaximumDistanceFromOriginAndMinimumWeight_
WeightedVectorSpace<dimensionality>::
    DetermineMaximumDistanceFromOriginAndMinimumWeight(
        Tolerance const& tolerance) const {
  using std::placeholders::_1, std::placeholders::_2;

#ifndef NDEBUG
  try {
    utilities::numeric_operations::ThrowIfToleranceIsNegative(tolerance);
  } catch (InvalidArgument const& exception) {
    Throw(exception,
          "bsplinelib::vector_spaces::WeightedVectorSpace::"
          "DetermineMaximumDistanceFromOriginAndMinimumWeight");
  }
#endif
  Coordinate maximum_distance{};
  Weight minimum_weight{std::numeric_limits<Weight>::max()};
  for (HomogeneousCoordinate_ const& homogeneous_coordinate :
       Base_::coordinates_) {
    maximum_distance = std::max(utilities::containers::TwoNorm(
                                    Project(homogeneous_coordinate)),
                                maximum_distance);
    minimum_weight =
        std::min(homogeneous_coordinate[dimensionality], minimum_weight);
  }
  return {maximum_distance, minimum_weight};
}

template<int dimensionality>
typename WeightedVectorSpace<dimensionality>::OutputInformation_
WeightedVectorSpace<dimensionality>::WriteProjected(
    Precision const& precision) const {
  using std::tuple_element_t;
  using ProjectedCoordinatesOutput = tuple_element_t<0, OutputInformation_>;
  using utilities::string_operations::Write;

  HomogeneousCoordinates_ const& homogeneous_coordinates = Base_::coordinates_;
  int const& number_of_homogeneous_coordinates = homogeneous_coordinates.size();
  ProjectedCoordinatesOutput coordinates;
  coordinates.reserve(number_of_homogeneous_coordinates);
  tuple_element_t<1, OutputInformation_> weights;
  weights.reserve(number_of_homogeneous_coordinates);
  std::for_each(homogeneous_coordinates.begin(),
                homogeneous_coordinates.end(),
                [&](HomogeneousCoordinate_ const& homogeneous_coordinate) {
                  coordinates.emplace_back(
                      Write<typename ProjectedCoordinatesOutput::value_type>(
                          Project(homogeneous_coordinate),
                          precision));
                  weights.emplace_back(
                      Write(homogeneous_coordinate[dimensionality], precision));
                });
  return OutputInformation_{coordinates, weights};
}

template<int dimensionality>
typename WeightedVectorSpace<dimensionality>::OutputInformation_
WeightedVectorSpace<dimensionality>::WriteWeighted(
    Precision const& precision) const {
  using std::tuple_element_t;
  using ProjectedCoordinatesOutput = tuple_element_t<0, OutputInformation_>;
  using utilities::string_operations::Write;

  HomogeneousCoordinates_ const& homogeneous_coordinates = Base_::coordinates_;
  int const& number_of_homogeneous_coordinates = homogeneous_coordinates.size();
  ProjectedCoordinatesOutput coordinates;
  coordinates.reserve(number_of_homogeneous_coordinates);
  tuple_element_t<1, OutputInformation_> weights;
  weights.reserve(number_of_homogeneous_coordinates);
  std::for_each(
      homogeneous_coordinates.begin(),
      homogeneous_coordinates.end(),
      [&](HomogeneousCoordinate_ const& homogeneous_coordinate) {
        coordinates.emplace_back(
            Write<typename ProjectedCoordinatesOutput::value_type>(
                [&]() {
                  Coordinate_ coordinate{};
                  Index::ForEach(0, coordinate.size(), [&](Index const& i) {
                    coordinate[i] = homogeneous_coordinate[i];
                  });
                  return coordinate;
                }(),
                precision));
        weights.emplace_back(
            Write(homogeneous_coordinate[dimensionality], precision));
      });
  return OutputInformation_{coordinates, weights};
}

template<int dimensionality>
typename WeightedVectorSpace<dimensionality>::HomogeneousCoordinates_
WeightedVectorSpace<dimensionality>::HomogenizeCoordinates(
    Coordinates_ const& coordinates,
    Weights_ const& weights) const {
  using std::to_string;

  int const& number_of_coordinates = coordinates.size();
#ifndef NDEBUG
  int const& number_of_weights = weights.size();
  if (number_of_coordinates != number_of_weights)
    throw DomainError(
        to_string(number_of_weights) + " weights were provided but "
        + to_string(number_of_coordinates)
        + " are needed to associate each weight with a coordinate.");
#endif
  HomogeneousCoordinates_ homogeneous_coordinates;
  homogeneous_coordinates.reserve(number_of_coordinates);
  Index::ForEach(0, number_of_coordinates, [&](Index const& coordinate_index) {
    Index::Type_ const& coordinate_index_value = coordinate_index.Get();
    Coordinate_ const& coordinate = coordinates[coordinate_index_value];
    HomogeneousCoordinate_ homogeneous_coordinate;
    Coordinate& weight = homogeneous_coordinate[dimensionality];
    weight = weights[coordinate_index_value];
    std::transform(coordinate.begin(),
                   coordinate.end(),
                   homogeneous_coordinate.begin(),
                   std::bind(std::multiplies{}, weight, std::placeholders::_1));
    homogeneous_coordinates.push_back(homogeneous_coordinate);
  });
  return homogeneous_coordinates;
}
