mod dfs;
pub mod error;
pub mod groups;
pub mod plugins;
mod validate;
pub mod vertex;

#[cfg(test)]
mod test {
    use super::plugins::SortingPlugin;
    use crate::error::PluginDataError;

    #[derive(Default)]
    pub struct TestPlugin {
        name: String,
        masters: Vec<String>,
        pub(super) is_master: bool,
        pub(super) is_blueprint_plugin: bool,
        pub(super) override_record_count: usize,
        pub(super) asset_count: usize,
        overlapping_record_plugins: Vec<String>,
        overlapping_asset_plugins: Vec<String>,
    }

    impl TestPlugin {
        pub fn new(name: &str) -> Self {
            Self {
                name: name.to_owned(),
                ..Default::default()
            }
        }

        pub fn add_master(&mut self, plugin_name: &str) {
            self.masters.push(plugin_name.to_owned());
        }

        pub fn add_overlapping_records(&mut self, plugin_name: &str) {
            self.overlapping_record_plugins.push(plugin_name.to_owned());
        }

        pub fn add_overlapping_assets(&mut self, plugin_name: &str) {
            self.overlapping_asset_plugins.push(plugin_name.to_owned());
        }
    }

    impl SortingPlugin for TestPlugin {
        fn name(&self) -> &str {
            &self.name
        }

        fn is_master(&self) -> bool {
            self.is_master
        }

        fn is_blueprint_plugin(&self) -> bool {
            self.is_blueprint_plugin
        }

        fn masters(&self) -> Result<Vec<String>, PluginDataError> {
            Ok(self.masters.clone())
        }

        fn override_record_count(&self) -> Result<usize, PluginDataError> {
            Ok(self.override_record_count)
        }

        fn asset_count(&self) -> usize {
            self.asset_count
        }

        fn do_records_overlap(&self, other: &Self) -> Result<bool, PluginDataError> {
            Ok(self.overlapping_record_plugins.contains(&other.name))
        }

        fn do_assets_overlap(&self, other: &Self) -> bool {
            self.overlapping_asset_plugins.contains(&other.name)
        }
    }
}
