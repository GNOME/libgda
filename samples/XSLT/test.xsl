<xsl:stylesheet version = '1.0'
     xmlns:xsl='http://www.w3.org/1999/XSL/Transform'
     xmlns:sql="http://www.gnome-db.org/ns/gda-sql-ext">

<xsl:template match="/">
<!-- The start template -->
<!-- now only one connection is posible to use -->
<h1>
<sql:section>
  <!-- no predefined query are registred, so use the sql text inside the node -->
  <sql:query name="customers">
  select * from customers
  where
  salesrep = ##srep::gint
  </sql:query>
  <!-- parameters login and password are set on XSLT ($login,...) -->
  <sql:template>
  <!-- !!! sql:template allows only
        ___ONE___ xsl:call_template element (optionally with <xsl:params.. inside)
  -->
  <xsl:call-template name="customerData"/>
  </sql:template>
</sql:section>
  <!-- not implemented, but here the query 'user' will not be avalible-->
</h1>
</xsl:template>

<xsl:template name="customerData">
<ul>
  <li>Id: <xsl:value-of select="sql:getvalue('customers','id')"/></li>
  <li>Name: <xsl:value-of select="sql:getvalue('customers','name')"/></li>
</ul>
</xsl:template>

</xsl:stylesheet>
