// Nicolò Grilli
// Università di Bristol
// 24 Giugno 2025

#include "DensityUpdateTwoSensitivities.h"
#include <algorithm>

registerMooseObject("c_pfor_amApp", DensityUpdateTwoSensitivities);

InputParameters
DensityUpdateTwoSensitivities::validParams()
{
  InputParameters params = ElementUserObject::validParams();
  params.addClassDescription(
      "Compute updated densities based on two sensitivities using an optimality criteria method to "
      "keep the volume constraint satisified. Specifically developed for multi-objective optimization "
      "of a mechanical + a thermomechanical simulation. ");
  params.addRequiredCoupledVar("design_density", "Design density variable name.");
  params.addRequiredParam<VariableName>("density_sensitivity",
                                        "Name of the density_sensitivity variable.");
  params.addRequiredParam<Real>("volume_fraction", "Volume Fraction");
  params.addRequiredParam<VariableName>("tm_sensitivity",
                                        "Name of the second sensitivity variable.");
  params.addParam<Real>("bisection_lower_bound", 0, "Lower bound for the bisection algorithm.");
  params.addParam<Real>("bisection_upper_bound", 1e16, "Upper bound for the bisection algorithm.");
  return params;
}

DensityUpdateTwoSensitivities::DensityUpdateTwoSensitivities(const InputParameters & parameters)
  : ElementUserObject(parameters),
    _mesh(_subproblem.mesh()),
    _density_sensitivity_name(getParam<VariableName>("density_sensitivity")),
    _design_density(&writableVariable("design_density")),
    _density_sensitivity(&_subproblem.getStandardVariable(_tid, _density_sensitivity_name)),
    _volume_fraction(getParam<Real>("volume_fraction")),
    _tm_sensitivity_name(getParam<VariableName>("tm_sensitivity")),
    _tm_sensitivity(&_subproblem.getStandardVariable(_tid, _tm_sensitivity_name)),
    _lower_bound(getParam<Real>("bisection_lower_bound")),
    _upper_bound(getParam<Real>("bisection_upper_bound"))
{
  if (!dynamic_cast<MooseVariableFE<Real> *>(_design_density))
    paramError("design_density", "Design density must be a finite element variable");
}

void
DensityUpdateTwoSensitivities::timestepSetup()
{
  gatherElementData();
  performOptimCritLoop();
}

void
DensityUpdateTwoSensitivities::execute()
{
  // Grab the element data for each id
  auto elem_data_iter = _elem_data_map.find(_current_elem->id());

  // Check if the element data is not null
  if (elem_data_iter != _elem_data_map.end())
  {
    ElementData & elem_data = elem_data_iter->second;
    dynamic_cast<MooseVariableFE<Real> *>(_design_density)->setNodalValue(elem_data.new_density);
  }
  else
  {
    mooseError("Element data not found for the current element id.");
  }
}

void
DensityUpdateTwoSensitivities::gatherElementData()
{
  _elem_data_map.clear();
  _total_allowable_volume = 0;

  for (const auto & sub_id : blockIDs())
    for (const auto & elem : _mesh.getMesh().active_local_subdomain_elements_ptr_range(sub_id))
    {
      dof_id_type elem_id = elem->id();
      ElementData data = ElementData(
          dynamic_cast<MooseVariableFE<Real> *>(_design_density)->getElementalValue(elem),
          dynamic_cast<const MooseVariableFE<Real> *>(_density_sensitivity)
              ->getElementalValue(elem),
          dynamic_cast<const MooseVariableFE<Real> *>(_tm_sensitivity)
              ->getElementalValue(elem),
          elem->volume(),
          0);
      _elem_data_map[elem_id] = data;
      _total_allowable_volume += elem->volume();
    }

  _communicator.sum(_total_allowable_volume);
  _total_allowable_volume *= _volume_fraction;
}

void
DensityUpdateTwoSensitivities::performOptimCritLoop()
{
  // Initialize the lower and upper bounds for the bisection method
  Real l1 = _lower_bound;
  Real l2 = _upper_bound;
  bool perform_loop = true;
  // Loop until the relative difference between l1 and l2 is less than a small tolerance
  while (perform_loop)
  {
    // Compute the midpoint between l1 and l2
    Real lmid = 0.5 * (l2 + l1);

    // Initialize the current total volume
    Real curr_total_volume = 0;
    // Loop over all elements
    for (auto && [id, elem_data] : _elem_data_map)
    {
      // Compute the updated density for the current element
      Real new_density = computeUpdatedDensity(elem_data.old_density, elem_data.sensitivity, elem_data.tm_sensitivity, lmid);
      // Update the current filtered density for the current element
      elem_data.new_density = new_density;
      // Update the current total volume
      curr_total_volume += new_density * elem_data.volume;
    }

    // Sum the current total volume across all processors
    _communicator.sum(curr_total_volume);

    // Update l1 or l2 based on whether the current total volume is greater than the total
    // allowable volume
    if (curr_total_volume > _total_allowable_volume)
      l1 = lmid;
    else
      l2 = lmid;

    // Determine whether to continue the loop based on the relative difference between l1 and l2
    perform_loop = (l2 - l1) / (l1 + l2) > 1e-3;
  }
}

// Method to compute the updated density for an element
Real
DensityUpdateTwoSensitivities::computeUpdatedDensity(Real current_density, Real dc, Real tm_sensitivity, Real lmid)
{
  // Define the maximum allowable change in density
  Real move = 0.1;
  // Compute the updated density based on the current density, the sensitivity, and the midpoint
  // value
  Real updated_density = std::max(
      0.0,
      std::min(1.0,
               std::min(current_density + move,
                        std::max(current_density - move,
                                 std::max(1e-5, current_density) * std::sqrt(-(dc+tm_sensitivity) / lmid)))));
  // Return the updated density
  return updated_density;
}
