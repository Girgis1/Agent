import unreal

mesh_path = '/Game/Drone/DroneImport/DroneCore1.DroneCore1'
mesh = unreal.load_asset(mesh_path)
if not mesh:
    raise RuntimeError(f'Failed to load mesh: {mesh_path}')

nanite_settings = mesh.get_editor_property('nanite_settings')
if nanite_settings.enabled:
    nanite_settings.enabled = False
    mesh.set_editor_property('nanite_settings', nanite_settings)
    mesh.modify()
    unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    unreal.log('Disabled Nanite on DroneCore1 and saved asset.')
else:
    unreal.log('Nanite already disabled on DroneCore1.')
