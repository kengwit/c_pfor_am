// Nicolò Grilli
// Università di Bristol
// 23 Novembre 2024

#include "SubdomainFileReader.h"
#include "CastUniquePointer.h"

#include "libmesh/elem.h"

registerMooseObject("c_pfor_amApp", SubdomainFileReader);

InputParameters
SubdomainFileReader::validParams()
{
  InputParameters params = MeshGenerator::validParams();

  params.addRequiredParam<MeshGeneratorName>("input", "The mesh we want to modify");
  params.addRequiredParam<FileName>(
      "subdomain_id_file_name",
      "Name of the file containing the subdomain ID for each element");
  params.addClassDescription("Subdomain ID for each element read from file for a structured mesh");
  return params;
}

SubdomainFileReader::SubdomainFileReader(const InputParameters & parameters)
  : MeshGenerator(parameters), 
  _input(getMesh("input")),
  _subdomain_id_file_name(getParam<FileName>("subdomain_id_file_name"))
{
}

std::unique_ptr<MeshBase>
SubdomainFileReader::generate()
{
  std::unique_ptr<MeshBase> mesh = std::move(_input);
  
  // read in the subdomain IDs from text file
  MooseUtils::DelimitedFileReader _reader(_subdomain_id_file_name);
  _reader.setFormatFlag(MooseUtils::DelimitedFileReader::FormatFlag::ROWS);
  _reader.read();

  // add check that the file has the same dimension as number of elements
  std::cout << mesh->max_elem_id() << std::endl;

  std::vector<SubdomainID> bids; //= getParam<std::vector<SubdomainID>>("subdomain_ids");

  // Generate a list of elements to which new subdomain IDs are to be assigned
  std::vector<Elem *> elements;
  
  std::vector<dof_id_type> elemids; // = getParam<std::vector<dof_id_type>>("element_ids");
  
  // assign element ids in increasing order
  for (const auto i : make_range(_reader.getData().size())) {
  
    elemids.push_back(i);
    
    // subdomain ID file has a single column
    bids.push_back(_reader.getData(i)[0]);
  
  }

  for (const auto & dof : elemids)
  {
    Elem * elem = mesh->query_elem_ptr(dof);
    bool has_elem = elem;

    // If no processor sees this element, something must be wrong
    // with the specified ID.  If another processor sees this
    // element but we don't, we'll insert NULL into our elements
    // vector anyway so as to keep the indexing matching our bids
    // vector.
    this->comm().max(has_elem);
    if (!has_elem)
      mooseError("invalid element ID is in element_ids");
    else
      elements.push_back(elem);
  }

  if (bids.size() != elements.size())
    mooseError(" Size of subdomain_ids is not consistent with the number of elements");

  // Assign new subdomain IDs and make sure elements in different types are not assigned with the
  // same subdomain ID
  std::map<ElemType, std::set<SubdomainID>> type2blocks;
  for (dof_id_type e = 0; e < elements.size(); ++e)
  {
    // Get the element we need to assign, or skip it if we just have a
    // nullptr placeholder indicating a remote element.
    Elem * elem = elements[e];
    if (!elem)
      continue;

    ElemType type = elem->type();
    SubdomainID newid = bids[e];

    bool has_type = false;
    for (auto & it : type2blocks)
    {
      if (it.first == type)
      {
        has_type = true;
        it.second.insert(newid);
      }
      else if (it.second.count(newid) > 0)
        mooseError("trying to assign elements with different types with the same subdomain ID");
    }

    if (!has_type)
    {
      std::set<SubdomainID> blocks;
      blocks.insert(newid);
      type2blocks.insert(std::make_pair(type, blocks));
    }

    elem->subdomain_id() = newid;
  }

  mesh->set_isnt_prepared();
  return dynamic_pointer_cast<MeshBase>(mesh);
}
