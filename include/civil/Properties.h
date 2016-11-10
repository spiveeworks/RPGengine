
typedef unsigned char byte;

// goals:
// output object data as compact bytes
// take compact bytes and make object + data

class PropertyFormat 
{
	typedef std::pair<byte, byte> element;
	typedef std::vector<element> format_type;
	typedef std::vector<PropertyFormat*> family_type;
	typedef format_type::size_type index;
    typedef std::map<index, family_type> family_lines_type;
	
	PropertyFormat const &parent;
	index parent_index;
	
	format_type format;
	family_lines_type family_lines;
	
	PropertyFormat* child (index line_index, byte child_id)
	{
		auto family_it = family_lines.find(line_index);
		return (family_it == family_lines.end()) ? nullptr :family_it->second[child_it];
	}
};
template<class el_t, class branch_t = PropertyFormat*>
using leaf_struct = std::map<branch_t, std::vector<el_t>>;
template<class el_t, class variant_t, class branch_t = PropertyFormat*>
using variant_leaf_struct = leaf_struct<el_t, std::pair<branch_t, variant_t>>;

struct Property
{
	typedef PropertyFormat::index index;
	typedef leaf_struct<byte> data_type;  // usage: byte datum = data[&branch_format][datum_index];
	data_type data;
	
	void erase(PropertyFormat* del)
	{
		if (!data.erase(del))
			return;
		std::vector<data_type::iterator> to_recurse = {del};
		while (to_recurse) 												// while there is still something on which to recurse
			for (auto& p: to_recurse.pop_back()->first->family_lines)	// remove one thing to recurse, and, for each possible group of children
				for (PropertyFormat* next: p->second)					// in fact for each child
					if (data.erase(next))								// search for and delete the branch associated with that child
						todel.push_back(next);							// and recurse... unless the child wasn't found anyway
	}
	void check_branch(PropertyFormat* check)
	{
		if (property->data.count(check))
			return;
		for (PropertyFormat* sibling: check->parent->family_lines[check->parent_index])
			erase_branch(sibling);
		data[check].resize(check->format.size());
	}
	template<class in_it_t>
	in_it_t read_in(in_it_t in_it, PropertyFormat* branch)
	{
	    check_branch(branch);
	    std::vector<byte> &branch_data = data[branch];
	    auto out_it = branch_data.begin();
	    for (std::pair<index, std::vector<PropertyFormat*> >& family: branch->family_lines)
	    {
	        while(out_it != branch_data.begin() + family.first + 1)
	            *out_it++ = *in_it++; // I love C++
	        in_it = read_in(in_it, family.second[branch_data[family.first]])
	    }
        while(out_it != branch_data.end())
            *out_it++ = *in_it++;
        return in_it;
	}
	
	/* recommended to use 
	 * 
	 * std::vector<byte> data;
	 * write_out(std::back_inserter(data), root_branch)
	 */
	template<class out_it_t>
	out_it_t write_out(out_it_t out_it, PropertyFormat* branch) const
	{
	    std::vector<byte> &branch_data = data.at(branch);
	    auto in_it = branch_data.begin();
	    for (std::pair<index, std::vector<PropertyFormat*> >& family: branch->family_lines)
	    {
	        out_it = std::copy(in_it, branch_data.begin() + family.first + 1, out_it)
	        out_it = write_out(out_it, family.second[branch_data[family.first]])
	    }
        return std::copy(in_it, branch_data.end(), out_it);
	}
	
	
	
};

struct datum {
    byte value = 0;
    PropertyFormat *branch = nullptr;
    byte operator()(Property const &base)
        {return branch ? base.data.at(branch)[value] : value;}
};

struct comparison {
    static EQU = 1,
           LSS = 2;
           GTR = 4;
         UNDEF = 8;
    char flags; // note only a nibble is used
    datum ld, rd; // left and right data
    bool operator()(Property const &base)
    {
        byte lv, rv;
      try
      {
        lv = ld(base);
        rv = rd(base);
      }
      catch(stuph)
        return flags & UNDEF;
        
        if (lv == rv)
            return flags & EQU;
        if (lv < rv)
            return flags & LSS;
        return flags & GTR;
    }
};

struct datum_template {
    struct conditioned_datum {
        datum out;
        std::vector<comparison> conditions;
        bool test(Property const& base) // maybe EAFTP? not literally but the style here is inconsistent otherwise
        {
            for (coparison& condition: conditions)
                if (!condition(base))
                    return false;
            return true;
        }
    };
    std::vector<conditioned_datum> possibilities;
    byte operator()(Property const &base)
//need to make sure that if a datum is invalid in spite of its conditions working, the next datum is tried
    {
        for (conditioned_datum& possibility: possibilities)
            if (possibility.test(base))
                return possibility.out(base);
        throw ranouttavalueslol;
    }
};

struct PropertyTemplate {
    struct branch_template {
        PropertyFormat *output_format;
        std::vector<datum_template> elements;
        std::vector<byte> operator()(Property const &base)
        {
            std::vector<byte> ret;
            ret.reserve(output_format->format.size());
            for (datum_template& element: elements)
                ret.push_back(element(base))
            return ret;
        }
    };
    struct stack_entry {
        std::vector<branch_template>::size_type current_branch_ind;
        PropertyFormat::family_lines_type::iterator current_line;
        std::vector<datum_template::conditioned_datum>::iterator current_child_branch;
        stack_entry() = default;
        stack_entry(stack_entry const &) = default;
        stack_entry(std::vector<branch_template>::size_type current_branch_ind_c, std::vector<branch_template> &branches, Property &output)
        {
            current_branch_ind = current_branch_ind_c;
            output_data(output).clear();
            output_data(output).reserve(current_branch().output_format->format.size());
            current_line = family_lines().begin();
            current_child_branch = current_child_branches().begin();
        }
        branch_template& current_branch() const
            {return branches[current_branch_ind];}
        std::vector<byte>& output_data(Property &output) const
            {return output.data[current_branch().output_format];}
        PropertyFormat::family_lines_type& family_lines() const
            {return current_branch().output_format->family_lines;}
        std::vector<datum_template::conditioned_datum>& current_child_branches () const
            {return current_branch.elements[current_line->first];}
        std::vector<datum_template>::size_type prev_element() const
        {
            if (current_line == family_lines.begin()) 
                return 0;
            else
            {
                PropertyFormat::family_lines_type::iterator X(current_line);
                return (--X)->first + 1;
            }
        }
        std::vector<datum_template>::size_type next_element() const
        {
            if (current_line == family_lines.end()) 
                return current_branch().output_format->format.size();
            else
                return current_line->first;
        }
        byte child_id() const
        {
            for (byte x = 0; x < current_line->second.size(); ++x)
                if (branches[(*current_child_branch)(base)].output_format == current_line->second[x])
                    return x;
            throw bad_child_branch_rip
        }
        void find_child_branch(Property const &base)
        {
            while 
            (
              current_child_branch != current_child_branches().end() 
              && 
              (
                !current_child_branch->test(base) 
                || current_child_branch->out.test(base)
              )
            )
                ++current_child_branch;
        }
        void next_child_branch(Property const &base)
        {
            ++current_child_branch;
            find_child_branch(base);
        }

    };
    std::vector<branch_template> branches;
    std::vector<branch_template>::size_type root_num;
    Property operator()(Property const &base)
    {
        std::stack<stack_entry> entries;
        Property ret;
        std::vector<branch_template>::size current_root = 0;
        while (entries || current_root < root_num)
        {
        	 try
        	 {
        	   if (!entries)
                stack_entry.emplace(current_root++, branches, ret);
            for (current_element = entries.top().prev_element(); current_element < entries.top().next_element(); ++current_element)
                entries.top().output_data(ret).push_back(entries.top().current_branch().elements[current_element](base));
            if (entries.top().current_line != entries.top().family_lines().end())
            {
                entries.top().find_child();
                if (entries.top().current_child_branch == entries.top().current_child_branches().end())
                    throw ranouttapossibilities;
                entries.emplace((*entries.top().current_child_branch)(base), branches, ret);
            }
            else
            {
                entries.pop();
                if (!entries)
                    break;
                entries.top().output_data(ret).push_back(entries.top().child_id());
                ++entries.top().current_line;
            }
          }
          catch (stuph)
          {
          	 do
          	 {
          	 	   ret.data.erase(entries.top().current_branch().output_format);
          	 	   entries.pop();
          	 	   if (entries)
          	 	       entries.top().next_child_branch();
          	 } while (entries && entries.top().current_child_branch != entries.top().current_child_branches().end());
          }
        }
        return ret;
    }
}

struct PropertyTemplate {
    //comp_flags = powerset(UNDEF | LSS | GTR | EQU)
    //datum = <(substruct_format, substruct index),literal_datum>
    //conditions = (comp_flags, datum_A, datum_B)
    //structure = (format, [datum,[conditions]])
    //template = [structures]
    variant_leaf_struct<std::vector<leaf_index>, int> format; // vector? why not forward_list?
    std::map<PropertyFormat*, int> width;
    inline std::vector<leaf_index>& op_branch (PropertyFormat* branch, int variant)
        {return format.at(std::make_pair(branch, variant));}
    
    PropertyTemplate(variant_leaf_struct<std::vector<leaf_index>, int> format_c)
    {
    	swap(format, format_c); // copy swap
    	for(std::pair<std::pair<PropertyFormat*, int>, std::vector<std::vector<leaf_index>>> const &p: format)
    	{
    		PropertyFormat* child = p.first.first;
    		p_width = width[child];
    		for (std::pair<index, std::vector<PropertyFormat*>> &family: child->family_lines)
    		    p_width = std::max(p_width, p.second[family.first].size()); // note that if forward_list is used instead for these then distance will be needed
    		width[child] = p_width;
    	}
    }
};

/* PropertyTemplate
 * maps std::pair<PropertyFormat*, int> to an iterable over leaf_index
 * each leaf_index is a possible way of interpretting an outout byte in terms of an input byte
 * elements that don't have branches can describe sources of bytes from a base property
 * elements that do have branches can describe branches to attempt
 * branches can be attempted multiple times in multiple ways
 *
 * variant ids of each child are:
 * parent_variant * child_variant_count + child_index
 * where parent_variant is the variant calculated for the parent branch,
 * child_variant_count is the highest number of child variants that a single parent associates with the relevant family line
 * child_index is particular to a child on a per parent basis. child_endex must be less than child_variant count
 *
 * child_variant_count can be found using template.width.at(parent)[line]
 * where line is the number of family lines at indeces below the desired family line
 * parent is the PropertyFormat* of the parent
 * template is a PropertyTemplate object
 *
 */


bool apply_leaf_indeces(std::vector<byte>& out, Property const &base, std::vector<std::vector<leaf_index> > const &op_branch, index copy_from, index copy_to)
{// my attempt at making the code less monstrous by making two functions didn't do so well
    index curr_size = out.size();
    out.resize(curr_size + copy_to - copy_from);
    auto out_it = out.end() - copy_to;  // such that out_it[copy_from] will currently refer to the first of the new bytes
    while(copy_from < copy_to)
    {
        bool success = false;
        for (leaf_index p: op_branch[copy_from])
        {
            if (!p.first)
            {
                out_it[copy_from] = p.second;
                success = true;
                break;
            }
            auto branch_it = base.data.find(p.first);
            if branch_it == base.data.end()
                continue;
            out = (*branch_it)[p.second];
            return true;
        }
        if(!success)
            return false;
        copied++;
    }
}

bool apply_template(std::vector<byte>& out, Property const &base, PropertyTemplate const &op_tree, PropertyFormat* branch, int branch_variant)
{// lol good luck debugging
    using index = std::vector<byte>::size_type;
    
    index const prev_size = out.size();
    auto const &op_branch = op_tree.op_branch(branch, branch_variant);
    int const root_variant = branch_variant * op_tree.width.at(branch); // this way parent's variants won't overlap with parent
    
    index copied = 0;
    for (std::pair<index, std::vector<PropertyFormat*> >& family: branch->family_lines)
    {
        if (!apply_leaf_indeces(out, base, op_branch, copied, family.first))
        {
            out.resize(prev_size);
            return false;
        }
        copied = family.first();
        
        bool child_success = false;
        int child_variant = root_variant;
        for(leaf_index p: op_branch[copied])
        {
            //special meaning if (p.first)?
            out.push_back(p.second);
            child_success = apply_template(out, base, op_tree, family.second[copied], child_variant++);
            if (child_success)
                break;
            out.pop_back();
        }
        if(!child_success)
        {
            out.resize(prev_size);
            return false;
        }
    }
    if (!apply_leaf_indeces(out, base, op_branch, copied, op_branch.size()))
    {
        out.resize(prev_size);
        return false;
    }
    return true;
}