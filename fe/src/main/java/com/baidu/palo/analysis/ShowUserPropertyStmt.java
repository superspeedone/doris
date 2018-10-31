// Copyright (c) 2017, Baidu.com, Inc. All Rights Reserved

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

package com.baidu.palo.analysis;

import com.baidu.palo.catalog.Catalog;
import com.baidu.palo.catalog.Column;
import com.baidu.palo.catalog.ColumnType;
import com.baidu.palo.cluster.ClusterNamespace;
import com.baidu.palo.common.AnalysisException;
import com.baidu.palo.common.CaseSensibility;
import com.baidu.palo.common.ErrorCode;
import com.baidu.palo.common.ErrorReport;
import com.baidu.palo.common.UserException;
import com.baidu.palo.common.PatternMatcher;
import com.baidu.palo.common.proc.UserPropertyProcNode;
import com.baidu.palo.mysql.privilege.PrivPredicate;
import com.baidu.palo.qe.ConnectContext;
import com.baidu.palo.qe.ShowResultSetMetaData;

import com.google.common.base.Strings;
import com.google.common.collect.Lists;

import org.apache.logging.log4j.LogManager;
import org.apache.logging.log4j.Logger;

import java.util.List;

// Show Property Stmt
//  syntax:
//      SHOW PROPERTY [FOR user] [LIKE key pattern]
public class ShowUserPropertyStmt extends ShowStmt {
    private static final Logger LOG = LogManager.getLogger(ShowUserPropertyStmt.class);

    private String user;
    private String pattern;

    public ShowUserPropertyStmt(String user, String pattern) {
        this.user = user;
        this.pattern = pattern;
    }

    @Override
    public void analyze(Analyzer analyzer) throws AnalysisException, UserException {
        super.analyze(analyzer);
        if (Strings.isNullOrEmpty(user)) {
            user = analyzer.getQualifiedUser();
            // user can see itself's property, no need to check privs
        } else {
            user = ClusterNamespace.getFullName(getClusterName(), user);

            if (!Catalog.getCurrentCatalog().getAuth().checkGlobalPriv(ConnectContext.get(), PrivPredicate.GRANT)) {
                ErrorReport.reportAnalysisException(ErrorCode.ERR_SPECIFIC_ACCESS_DENIED_ERROR, "GRANT");
            }
        }

        pattern = Strings.emptyToNull(pattern);
    }

    public List<List<String>> getRows() throws AnalysisException {
        List<List<String>> rows = Catalog.getCurrentCatalog().getAuth().getUserProperties(user);

        if (pattern == null) {
            return rows;
        }

        List<List<String>> result = Lists.newArrayList();
        PatternMatcher matcher = PatternMatcher.createMysqlPattern(pattern,
                                                                   CaseSensibility.USER.getCaseSensibility());
        for (List<String> row : rows) {
            String key = row.get(0).split("\\" + SetUserPropertyVar.DOT_SEPARATOR)[0];
            if (matcher.match(key)) {
                result.add(row);
            }
        }

        return result;
    }

    @Override
    public ShowResultSetMetaData getMetaData() {
        ShowResultSetMetaData.Builder builder = ShowResultSetMetaData.builder();
        for (String col : UserPropertyProcNode.TITLE_NAMES) {
            builder.addColumn(new Column(col, ColumnType.createVarchar(30)));
        }
        return builder.build();
    }

    @Override
    public String toSql() {
        StringBuilder sb = new StringBuilder();
        sb.append("SHOW PROPERTY FOR '");
        sb.append(user);
        sb.append("'");

        if (pattern != null) {
            sb.append(" LIKE '");
            sb.append(pattern);
            sb.append("'");
        }

        return sb.toString();
    }

    @Override
    public String toString() {
        return toSql();
    }
}
