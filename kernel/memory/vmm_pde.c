#include "vmm_pde.h"

void pd_entry_add_attrib (pd_entry* e, uint32_t attrib) {
	*e |= attrib;
}

void pd_entry_del_attrib (pd_entry* e, uint32_t attrib) {
	*e &= ~attrib;
}

void pd_entry_set_frame (pd_entry* e, physical_addr addr) {
	*e = (*e & ~I86_PDE_FRAME) | addr;
}

bool pd_entry_is_present (pd_entry e) {
	return e & I86_PDE_PRESENT;
}

bool pd_entry_is_writable (pd_entry e) {
	return e & I86_PDE_WRITABLE;
}

physical_addr pd_entry_pfn (pd_entry e) {
	return e & I86_PDE_FRAME;
}

bool pd_entry_is_user (pd_entry e) {
	return e & I86_PDE_USER;
}

bool pd_entry_is_4mb (pd_entry e) {
	return e & I86_PDE_4MB;
}

void pd_entry_enable_global (pd_entry e) {

}
